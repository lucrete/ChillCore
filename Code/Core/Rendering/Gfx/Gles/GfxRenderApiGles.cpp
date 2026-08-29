#include "GfxRenderApiGles.h"
#include "GlHandlePool.h"
#include "CCAssert.h"
#include "PrintManager.h"
#include "ObjectUniforms.h"
#include "PlatformWindow.h"

namespace CC::Gfx
{
    // ========================
    // Static translation tables
    // ========================

    static GLenum ToGlBufferTarget(BufferUsage usage)
    {
        GLenum result = GL_ARRAY_BUFFER;
        switch (usage)
        {
            case BufferUsage::Vertex:   result = GL_ARRAY_BUFFER;          break;
            case BufferUsage::Index:    result = GL_ELEMENT_ARRAY_BUFFER;  break;
            case BufferUsage::Uniform:  result = GL_UNIFORM_BUFFER;        break;
            case BufferUsage::Storage:  result = GL_SHADER_STORAGE_BUFFER; break;
            case BufferUsage::Indirect: result = GL_DRAW_INDIRECT_BUFFER;  break;
            default:
                CC_ASSERT(false, "Unknown BufferUsage");
                break;
        }
        return result;
    }

    static GLenum ToGlBufferUsageHint(BufferMemory memory)
    {
        GLenum result = GL_STATIC_DRAW;
        switch (memory)
        {
            case BufferMemory::GpuOnly:  result = GL_STATIC_DRAW;  break;
            case BufferMemory::CpuToGpu: result = GL_DYNAMIC_DRAW; break;
            case BufferMemory::GpuToCpu: result = GL_STREAM_READ;  break;
            default:
                CC_ASSERT(false, "Unknown BufferMemory");
                break;
        }
        return result;
    }

    struct GlTextureFormatInfo
    {
        GLenum internalFormat;
        GLenum pixelFormat;
        GLenum pixelType;
        bool   isCompressed;
        bool   isDepth;
    };

    static GlTextureFormatInfo ToGlTextureFormat(TextureFormat format)
    {
        // GLES 3.1 baseline formats. BC compressed entries are absent on
        // mobile (use ASTC instead) and assert here.
        GlTextureFormatInfo result = { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false };
        switch (format)
        {
            case TextureFormat::R8Unorm:         result = { GL_R8,                 GL_RED,             GL_UNSIGNED_BYTE,                false, false }; break;
            case TextureFormat::Rg8Unorm:        result = { GL_RG8,                GL_RG,              GL_UNSIGNED_BYTE,                false, false }; break;
            case TextureFormat::Rgba8Unorm:      result = { GL_RGBA8,              GL_RGBA,            GL_UNSIGNED_BYTE,                false, false }; break;
            case TextureFormat::Rgba8Srgb:       result = { GL_SRGB8_ALPHA8,       GL_RGBA,            GL_UNSIGNED_BYTE,                false, false }; break;
            case TextureFormat::Rgb10A2Unorm:    result = { GL_RGB10_A2,           GL_RGBA,            GL_UNSIGNED_INT_2_10_10_10_REV,  false, false }; break;
            case TextureFormat::R16Float:        result = { GL_R16F,               GL_RED,             GL_HALF_FLOAT,                   false, false }; break;
            case TextureFormat::Rg16Float:       result = { GL_RG16F,              GL_RG,              GL_HALF_FLOAT,                   false, false }; break;
            case TextureFormat::Rgba16Float:     result = { GL_RGBA16F,            GL_RGBA,            GL_HALF_FLOAT,                   false, false }; break;
            case TextureFormat::R32Float:        result = { GL_R32F,               GL_RED,             GL_FLOAT,                        false, false }; break;
            case TextureFormat::Rgba32Float:     result = { GL_RGBA32F,            GL_RGBA,            GL_FLOAT,                        false, false }; break;
            case TextureFormat::Bc1:
            case TextureFormat::Bc3:
            case TextureFormat::Bc7:
                CC_ASSERT(false, "BC compressed formats are not supported on GLES; use ASTC");
                break;
            case TextureFormat::Depth24Stencil8: result = { GL_DEPTH24_STENCIL8,   GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8,            false, true  }; break;
            case TextureFormat::Depth32Float:    result = { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT,                        false, true  }; break;
            default:
                CC_ASSERT(false, "Unsupported TextureFormat for GLES");
                break;
        }
        return result;
    }

    static GLenum ToGlMinFilter(FilterMode minFilter, MipmapMode mipmapMode)
    {
        GLenum result = GL_LINEAR;
        if (mipmapMode == MipmapMode::None)
        {
            result = (minFilter == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
        }
        else if (mipmapMode == MipmapMode::Nearest)
        {
            result = (minFilter == FilterMode::Nearest) ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
        }
        else
        {
            result = (minFilter == FilterMode::Nearest) ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
        }
        return result;
    }

    static GLenum ToGlMagFilter(FilterMode magFilter)
    {
        return (magFilter == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
    }

    static GLenum ToGlAddressMode(AddressMode mode)
    {
        // GLES base does not have GL_CLAMP_TO_BORDER (it's an EXT extension).
        // ClampToBorder falls back to ClampToEdge — visually similar in
        // most cases; revisit if engine code starts relying on the border
        // colour.
        GLenum result = GL_REPEAT;
        switch (mode)
        {
            case AddressMode::Repeat:         result = GL_REPEAT;          break;
            case AddressMode::MirroredRepeat: result = GL_MIRRORED_REPEAT; break;
            case AddressMode::ClampToEdge:    result = GL_CLAMP_TO_EDGE;   break;
            case AddressMode::ClampToBorder:  result = GL_CLAMP_TO_EDGE;   break;
            default: CC_ASSERT(false, "Unknown AddressMode"); break;
        }
        return result;
    }

    static GLenum ToGlCompareOp(CompareOp op)
    {
        GLenum result = GL_LESS;
        switch (op)
        {
            case CompareOp::Never:        result = GL_NEVER;    break;
            case CompareOp::Less:         result = GL_LESS;     break;
            case CompareOp::Equal:        result = GL_EQUAL;    break;
            case CompareOp::LessEqual:    result = GL_LEQUAL;   break;
            case CompareOp::Greater:      result = GL_GREATER;  break;
            case CompareOp::NotEqual:     result = GL_NOTEQUAL; break;
            case CompareOp::GreaterEqual: result = GL_GEQUAL;   break;
            case CompareOp::Always:       result = GL_ALWAYS;   break;
            default: CC_ASSERT(false, "Unknown CompareOp"); break;
        }
        return result;
    }

    static GLenum ToGlBlendFactor(BlendFactor factor)
    {
        GLenum result = GL_ONE;
        switch (factor)
        {
            case BlendFactor::Zero:             result = GL_ZERO;                break;
            case BlendFactor::One:              result = GL_ONE;                 break;
            case BlendFactor::SrcColor:         result = GL_SRC_COLOR;           break;
            case BlendFactor::OneMinusSrcColor: result = GL_ONE_MINUS_SRC_COLOR; break;
            case BlendFactor::DstColor:         result = GL_DST_COLOR;           break;
            case BlendFactor::OneMinusDstColor: result = GL_ONE_MINUS_DST_COLOR; break;
            case BlendFactor::SrcAlpha:         result = GL_SRC_ALPHA;           break;
            case BlendFactor::OneMinusSrcAlpha: result = GL_ONE_MINUS_SRC_ALPHA; break;
            case BlendFactor::DstAlpha:         result = GL_DST_ALPHA;           break;
            case BlendFactor::OneMinusDstAlpha: result = GL_ONE_MINUS_DST_ALPHA; break;
            default: CC_ASSERT(false, "Unknown BlendFactor"); break;
        }
        return result;
    }

    static GLenum ToGlBlendOp(BlendOp op)
    {
        GLenum result = GL_FUNC_ADD;
        switch (op)
        {
            case BlendOp::Add:             result = GL_FUNC_ADD;              break;
            case BlendOp::Subtract:        result = GL_FUNC_SUBTRACT;         break;
            case BlendOp::ReverseSubtract: result = GL_FUNC_REVERSE_SUBTRACT; break;
            case BlendOp::Min:             result = GL_MIN;                   break;
            case BlendOp::Max:             result = GL_MAX;                   break;
            default: CC_ASSERT(false, "Unknown BlendOp"); break;
        }
        return result;
    }

    static GLenum ToGlPrimitiveTopology(PrimitiveTopology topology)
    {
        GLenum result = GL_TRIANGLES;
        switch (topology)
        {
            case PrimitiveTopology::Points:        result = GL_POINTS;         break;
            case PrimitiveTopology::Lines:         result = GL_LINES;          break;
            case PrimitiveTopology::LineStrip:     result = GL_LINE_STRIP;     break;
            case PrimitiveTopology::Triangles:     result = GL_TRIANGLES;      break;
            case PrimitiveTopology::TriangleStrip: result = GL_TRIANGLE_STRIP; break;
            default: CC_ASSERT(false, "Unknown PrimitiveTopology"); break;
        }
        return result;
    }

    static GLenum ToGlIndexType(IndexType type)
    {
        return (type == IndexType::Uint16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    }

    static GLenum ToGlCullFace(CullMode mode)
    {
        return (mode == CullMode::Front) ? GL_FRONT : GL_BACK;
    }

    static GLenum ToGlVertexAttribType(VertexAttribType type)
    {
        GLenum result = GL_FLOAT;
        switch (type)
        {
            case VertexAttribType::Float32:   result = GL_FLOAT;          break;
            case VertexAttribType::Float16:   result = GL_HALF_FLOAT;     break;
            case VertexAttribType::Uint8Norm: result = GL_UNSIGNED_BYTE;  break;
            case VertexAttribType::Uint8:     result = GL_UNSIGNED_BYTE;  break;
            case VertexAttribType::Uint16:    result = GL_UNSIGNED_SHORT; break;
            case VertexAttribType::Uint32:    result = GL_UNSIGNED_INT;   break;
            case VertexAttribType::Int32:     result = GL_INT;            break;
            default: CC_ASSERT(false, "Unknown VertexAttribType"); break;
        }
        return result;
    }

    static GLbitfield ToGlBarrierBits(unsigned int barrierBits)
    {
        GLbitfield result = 0;
        if (barrierBits == BarrierBit::All)
        {
            result = GL_ALL_BARRIER_BITS;
        }
        else
        {
            if (barrierBits & BarrierBit::VertexAttribRead) result |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
            if (barrierBits & BarrierBit::IndexRead)        result |= GL_ELEMENT_ARRAY_BARRIER_BIT;
            if (barrierBits & BarrierBit::UniformRead)      result |= GL_UNIFORM_BARRIER_BIT;
            if (barrierBits & BarrierBit::TextureFetch)     result |= GL_TEXTURE_FETCH_BARRIER_BIT;
            if (barrierBits & BarrierBit::ImageAccess)      result |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
            if (barrierBits & BarrierBit::StorageBuffer)    result |= GL_SHADER_STORAGE_BARRIER_BIT;
            if (barrierBits & BarrierBit::IndirectCommand)  result |= GL_COMMAND_BARRIER_BIT;
            if (barrierBits & BarrierBit::BufferUpdate)     result |= GL_BUFFER_UPDATE_BARRIER_BIT;
            if (barrierBits & BarrierBit::Framebuffer)      result |= GL_FRAMEBUFFER_BARRIER_BIT;
        }
        return result;
    }

    // ========================
    // Lifecycle
    // ========================

    RenderApiGles::RenderApiGles()
    {
    }

    RenderApiGles::~RenderApiGles()
    {
    }

    void RenderApiGles::Init()
    {
        QueryCapabilities();
    }

    void RenderApiGles::Shutdown()
    {
        for (size_t i = 0; i < buffers.size(); i++)
        {
            if (buffers[i].isAlive && buffers[i].glHandle != 0)
            {
                glDeleteBuffers(1, &buffers[i].glHandle);
            }
        }
        buffers.clear();
        freeBufferSlots.clear();

        for (size_t i = 0; i < textures.size(); i++)
        {
            if (textures[i].isAlive && textures[i].glHandle != 0)
            {
                glDeleteTextures(1, &textures[i].glHandle);
            }
        }
        textures.clear();
        freeTextureSlots.clear();

        for (size_t i = 0; i < samplers.size(); i++)
        {
            if (samplers[i].isAlive && samplers[i].glHandle != 0)
            {
                glDeleteSamplers(1, &samplers[i].glHandle);
            }
        }
        samplers.clear();
        freeSamplerSlots.clear();

        for (size_t i = 0; i < shaders.size(); i++)
        {
            if (shaders[i].isAlive && shaders[i].program != 0)
            {
                glDeleteProgram(shaders[i].program);
            }
        }
        shaders.clear();
        freeShaderSlots.clear();

        for (size_t i = 0; i < pipelines.size(); i++)
        {
            if (pipelines[i].isAlive && pipelines[i].vao != 0)
            {
                glDeleteVertexArrays(1, &pipelines[i].vao);
            }
        }
        pipelines.clear();
        freePipelineSlots.clear();

        for (size_t i = 0; i < renderTargets.size(); i++)
        {
            if (renderTargets[i].isAlive && renderTargets[i].fbo != 0)
            {
                glDeleteFramebuffers(1, &renderTargets[i].fbo);
            }
        }
        renderTargets.clear();
        freeRenderTargetSlots.clear();

        if (pushConstantsUbo != 0)
        {
            glDeleteBuffers(1, &pushConstantsUbo);
            pushConstantsUbo = 0;
        }
    }

    const GfxCapabilities& RenderApiGles::GetCapabilities() const
    {
        return capabilities;
    }

    void RenderApiGles::QueryCapabilities()
    {
        GLint maxTextureSize        = 0;
        GLint maxVertexAttribs      = 0;
        GLint maxUniformBlockSize   = 0;
        GLint maxSsboSize           = 0;
        GLint maxColorAttachments   = 0;
        GLint maxSamples            = 0;
        GLint maxComputeX           = 0;
        GLint maxComputeY           = 0;
        GLint maxComputeZ           = 0;
        GLint maxComputeInvocations = 0;

        glGetIntegerv(GL_MAX_TEXTURE_SIZE,                &maxTextureSize);
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,              &maxVertexAttribs);
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,          &maxUniformBlockSize);
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS,           &maxColorAttachments);
        glGetIntegerv(GL_MAX_SAMPLES,                     &maxSamples);
        glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,   &maxSsboSize);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxComputeX);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxComputeY);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxComputeZ);
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxComputeInvocations);

        capabilities.supportsComputeShaders        = true;
        capabilities.supportsStorageBuffers        = true;
        capabilities.supportsIndirectDraw          = true;
        capabilities.supportsGeometryShader        = false;  // GLES 3.2 only.
        capabilities.supportsTessellation          = false;  // GLES 3.2 only.
        capabilities.supportsBcTextureFormats      = false;
        capabilities.supportsAstcTextureFormats    = true;
        capabilities.supportsAnisotropicFiltering  = false;  // EXT_texture_filter_anisotropic; not gated in v1.
        capabilities.supportsDebugMarkers          = false;  // KHR_debug; not gated in v1.
        capabilities.supportsGpuTimestamps         = false;  // EXT_disjoint_timer_query; not gated in v1.

        capabilities.maxTextureSize                 = maxTextureSize;
        capabilities.maxVertexAttributes            = maxVertexAttribs;
        capabilities.maxUniformBufferSizeBytes      = maxUniformBlockSize;
        capabilities.maxStorageBufferSizeBytes      = maxSsboSize;
        capabilities.maxColorAttachments            = maxColorAttachments;
        capabilities.maxMsaaSamples                 = maxSamples;
        capabilities.maxComputeWorkgroupSizeX       = maxComputeX;
        capabilities.maxComputeWorkgroupSizeY       = maxComputeY;
        capabilities.maxComputeWorkgroupSizeZ       = maxComputeZ;
        capabilities.maxComputeWorkgroupInvocations = maxComputeInvocations;

        capabilities.backendName   = "OpenGL ES";
        capabilities.deviceName    = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        capabilities.driverVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    }

    // ========================
    // Frame
    // ========================

    void RenderApiGles::BeginFrame()
    {
    }

    void RenderApiGles::EndFrame()
    {
        CC::PlatformWindow::Get()->SwapBuffers();
    }

    // ========================
    // Backbuffer
    // ========================

    void RenderApiGles::ConfigureBackbuffer(const BackbufferDescription& description)
    {
        backbufferDescription = description;
    }

    RenderTargetHandle RenderApiGles::GetBackbuffer() const
    {
        return RenderTargetHandle();
    }

    void RenderApiGles::GetBackbufferSize(int& width, int& height) const
    {
        int outWidth  = backbufferDescription.width;
        int outHeight = backbufferDescription.height;
        if (outWidth <= 0 || outHeight <= 0)
        {
            CC::PlatformWindow::Get()->GetFramebufferSize(outWidth, outHeight);
        }
        width  = outWidth;
        height = outHeight;
    }

    // ========================
    // Render pass
    // ========================
    //
    // GLES v1 path renders directly to the default framebuffer. No MSAA
    // scene FB and no end-pass blit; that complexity returns when the
    // feature genuinely lands.

    void RenderApiGles::BeginDefaultRenderPass(const float clearColor[4], float clearDepth)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glClearDepthf(clearDepth);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void RenderApiGles::BeginRenderPass(RenderTargetHandle target)
    {
        CC_ASSERT(!renderPassActive, "BeginRenderPass: a render pass is already active");

        if (target.IsValid())
        {
            uint32_t slotIndex = target.id;
            CC_ASSERT(slotIndex < renderTargets.size() && renderTargets[slotIndex].isAlive,
                      "BeginRenderPass: render target not alive");
            GlRenderTarget& entry = renderTargets[slotIndex];
            const RenderTargetDescription& desc = entry.description;

            glBindFramebuffer(GL_FRAMEBUFFER, entry.fbo);
            glViewport(0, 0, entry.width, entry.height);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);

            for (int i = 0; i < desc.colorAttachmentCount; i++)
            {
                if (desc.colorAttachments[i].loadOp == LoadOp::Clear)
                {
                    glClearBufferfv(GL_COLOR, i, desc.colorAttachments[i].clearColor);
                }
            }
            if (desc.hasDepthStencil && desc.depthStencilAttachment.loadOp == LoadOp::Clear)
            {
                glClear(GL_DEPTH_BUFFER_BIT);
            }

            currentRenderTarget    = target;
            currentPassIsOffscreen = true;
        }
        else
        {
            int width  = 0;
            int height = 0;
            GetBackbufferSize(width, height);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            currentRenderTarget    = RenderTargetHandle();
            currentPassIsOffscreen = false;
        }

        // The raw glBindFramebuffer / glClear above bypass the tracked binds.
        InvalidateCachedState();

        renderPassActive = true;
    }

    void RenderApiGles::EndRenderPass()
    {
        CC_ASSERT(renderPassActive, "EndRenderPass: no render pass is active");

        if (currentPassIsOffscreen)
        {
            // Offscreen targets are single-sample: no resolve, no blit.
            // Honour StoreOp::DontCare as a discard hint — valuable on the
            // tiled mobile GPUs this backend runs on.
            uint32_t slotIndex = currentRenderTarget.id;
            const RenderTargetDescription& desc = renderTargets[slotIndex].description;

            GLenum discard[MAX_COLOR_ATTACHMENTS + 1];
            int discardCount = 0;
            for (int i = 0; i < desc.colorAttachmentCount; i++)
            {
                if (desc.colorAttachments[i].storeOp == StoreOp::DontCare)
                {
                    discard[discardCount++] = GL_COLOR_ATTACHMENT0 + i;
                }
            }
            if (desc.hasDepthStencil && desc.depthStencilAttachment.storeOp == StoreOp::DontCare)
            {
                discard[discardCount++] = GL_DEPTH_ATTACHMENT;
            }
            if (discardCount > 0)
            {
                glInvalidateFramebuffer(GL_FRAMEBUFFER, discardCount, discard);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            currentRenderTarget    = RenderTargetHandle();
            currentPassIsOffscreen = false;
            InvalidateCachedState();
        }

        renderPassActive = false;
    }

    // ========================
    // Buffers
    // ========================

    BufferHandle RenderApiGles::CreateBuffer(const BufferDescription& description)
    {
        CC_ASSERT(description.sizeBytes > 0, "BufferDescription.sizeBytes must be > 0");

        GlBuffer entry;
        entry.target    = ToGlBufferTarget(description.usage);
        entry.usageHint = ToGlBufferUsageHint(description.memory);
        entry.sizeBytes = description.sizeBytes;
        entry.usage     = description.usage;
        entry.memory    = description.memory;
        entry.isAlive   = true;

        glGenBuffers(1, &entry.glHandle);
        glBindBuffer(entry.target, entry.glHandle);
        glBufferData(entry.target, description.sizeBytes, description.initialData, entry.usageHint);
        glBindBuffer(entry.target, 0);

        uint32_t slotIndex = GlCommon::AcquireSlot(freeBufferSlots, static_cast<uint32_t>(buffers.size()));
        if (slotIndex == buffers.size())
        {
            buffers.push_back(entry);
        }
        else
        {
            buffers[slotIndex] = entry;
        }

        BufferHandle handle;
        handle.id = slotIndex;
        return handle;
    }

    void RenderApiGles::DestroyBuffer(BufferHandle handle)
    {
        if (handle.IsValid())
        {
            uint32_t slotIndex = handle.id;
            CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
            GlBuffer& entry = buffers[slotIndex];
            if (entry.isAlive)
            {
                glDeleteBuffers(1, &entry.glHandle);
                entry = GlBuffer();
                freeBufferSlots.push_back(slotIndex);
            }
        }
    }

    void RenderApiGles::UpdateBuffer(BufferHandle handle, int offsetBytes, int sizeBytes, const void* data)
    {
        CC_ASSERT(handle.IsValid(), "UpdateBuffer: invalid handle");
        uint32_t slotIndex = handle.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "UpdateBuffer: buffer already destroyed");
        CC_ASSERT(offsetBytes + sizeBytes <= entry.sizeBytes, "UpdateBuffer: range exceeds buffer size");

        glBindBuffer(entry.target, entry.glHandle);
        glBufferSubData(entry.target, offsetBytes, sizeBytes, data);
        glBindBuffer(entry.target, 0);
    }

    void RenderApiGles::ReadBuffer(BufferHandle handle, int offsetBytes, int sizeBytes, void* destination)
    {
        // GLES 3.0+ does not have glGetBufferSubData. Use glMapBufferRange
        // for readback; rare path, only used by debug tools today.
        CC_ASSERT(handle.IsValid(), "ReadBuffer: invalid handle");
        uint32_t slotIndex = handle.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "ReadBuffer: buffer already destroyed");

        glBindBuffer(entry.target, entry.glHandle);
        const void* mapped = glMapBufferRange(entry.target, offsetBytes, sizeBytes, GL_MAP_READ_BIT);
        if (mapped != nullptr)
        {
            for (int i = 0; i < sizeBytes; i++)
            {
                static_cast<unsigned char*>(destination)[i] = static_cast<const unsigned char*>(mapped)[i];
            }
            glUnmapBuffer(entry.target);
        }
        glBindBuffer(entry.target, 0);
    }

    // ========================
    // Textures
    // ========================

    TextureHandle RenderApiGles::CreateTexture(const TextureDescription& description)
    {
        CC_ASSERT(description.width > 0 && description.height > 0, "TextureDescription: width/height must be > 0");
        CC_ASSERT(description.mipLevels >= 1, "TextureDescription: mipLevels must be >= 1");

        GlTextureFormatInfo formatInfo = ToGlTextureFormat(description.format);

        GlTexture entry;
        entry.format  = description.format;
        entry.width   = description.width;
        entry.height  = description.height;
        entry.isAlive = true;
        entry.target  = description.isCubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

        glGenTextures(1, &entry.glHandle);
        glBindTexture(entry.target, entry.glHandle);

        glTexStorage2D(entry.target, description.mipLevels, formatInfo.internalFormat, description.width, description.height);

        glTexParameteri(entry.target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(entry.target, GL_TEXTURE_MAX_LEVEL, description.mipLevels - 1);

        if (description.initialData != nullptr && !formatInfo.isCompressed)
        {
            if (description.isCubemap)
            {
                const unsigned char* facePtr = static_cast<const unsigned char*>(description.initialData);
                int faceSizeBytes = description.width * description.height * 4;
                for (int faceIndex = 0; faceIndex < 6; faceIndex++)
                {
                    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, 0, 0, 0,
                                    description.width, description.height,
                                    formatInfo.pixelFormat, formatInfo.pixelType,
                                    facePtr + faceIndex * faceSizeBytes);
                }
            }
            else
            {
                glTexSubImage2D(entry.target, 0, 0, 0,
                                description.width, description.height,
                                formatInfo.pixelFormat, formatInfo.pixelType,
                                description.initialData);
            }

            if (description.mipLevels > 1)
            {
                glGenerateMipmap(entry.target);
            }
        }

        glBindTexture(entry.target, 0);

        uint32_t slotIndex = GlCommon::AcquireSlot(freeTextureSlots, static_cast<uint32_t>(textures.size()));
        if (slotIndex == textures.size())
        {
            textures.push_back(entry);
        }
        else
        {
            textures[slotIndex] = entry;
        }

        TextureHandle handle;
        handle.id = slotIndex;
        return handle;
    }

    void RenderApiGles::DestroyTexture(TextureHandle handle)
    {
        if (handle.IsValid())
        {
            uint32_t slotIndex = handle.id;
            CC_ASSERT(slotIndex < textures.size(), "TextureHandle out of range");
            GlTexture& entry = textures[slotIndex];
            if (entry.isAlive)
            {
                glDeleteTextures(1, &entry.glHandle);
                entry = GlTexture();
                freeTextureSlots.push_back(slotIndex);
            }
        }
    }

    void RenderApiGles::UpdateTexture(TextureHandle handle, int mipLevel, int x, int y, int width, int height, const void* data)
    {
        CC_ASSERT(handle.IsValid(), "UpdateTexture: invalid handle");
        uint32_t slotIndex = handle.id;
        CC_ASSERT(slotIndex < textures.size(), "TextureHandle out of range");
        GlTexture& entry = textures[slotIndex];
        CC_ASSERT(entry.isAlive, "UpdateTexture: texture already destroyed");

        GlTextureFormatInfo formatInfo = ToGlTextureFormat(entry.format);

        glBindTexture(entry.target, entry.glHandle);
        glTexSubImage2D(entry.target, mipLevel, x, y, width, height,
                        formatInfo.pixelFormat, formatInfo.pixelType, data);
        glBindTexture(entry.target, 0);
    }

    // ========================
    // Samplers
    // ========================

    SamplerHandle RenderApiGles::CreateSampler(const SamplerDescription& description)
    {
        GlSampler entry;
        entry.isAlive = true;

        glGenSamplers(1, &entry.glHandle);
        glSamplerParameteri(entry.glHandle, GL_TEXTURE_MIN_FILTER, ToGlMinFilter(description.minFilter, description.mipmapMode));
        glSamplerParameteri(entry.glHandle, GL_TEXTURE_MAG_FILTER, ToGlMagFilter(description.magFilter));
        glSamplerParameteri(entry.glHandle, GL_TEXTURE_WRAP_S, ToGlAddressMode(description.addressU));
        glSamplerParameteri(entry.glHandle, GL_TEXTURE_WRAP_T, ToGlAddressMode(description.addressV));
        glSamplerParameteri(entry.glHandle, GL_TEXTURE_WRAP_R, ToGlAddressMode(description.addressW));

        // Anisotropic filtering is EXT_texture_filter_anisotropic on GLES;
        // unconditionally skipped in v1 because the capability is reported
        // off (no extension query yet).
        (void)description.maxAnisotropy;

        uint32_t slotIndex = GlCommon::AcquireSlot(freeSamplerSlots, static_cast<uint32_t>(samplers.size()));
        if (slotIndex == samplers.size())
        {
            samplers.push_back(entry);
        }
        else
        {
            samplers[slotIndex] = entry;
        }

        SamplerHandle handle;
        handle.id = slotIndex;
        return handle;
    }

    void RenderApiGles::DestroySampler(SamplerHandle handle)
    {
        if (handle.IsValid())
        {
            uint32_t slotIndex = handle.id;
            CC_ASSERT(slotIndex < samplers.size(), "SamplerHandle out of range");
            GlSampler& entry = samplers[slotIndex];
            if (entry.isAlive)
            {
                glDeleteSamplers(1, &entry.glHandle);
                entry = GlSampler();
                freeSamplerSlots.push_back(slotIndex);
            }
        }
    }

    // ========================
    // Shaders
    // ========================

    static const char* GlShaderStageName(GLenum stage)
    {
        const char* result = "Unknown";
        switch (stage)
        {
            case GL_VERTEX_SHADER:   result = "Vertex";   break;
            case GL_FRAGMENT_SHADER: result = "Fragment"; break;
            case GL_COMPUTE_SHADER:  result = "Compute";  break;
            default:                                      break;
        }
        return result;
    }

    static GLuint CompileGlShaderStage(GLenum stage, const char* source, const char* debugName)
    {
        GLuint glShader = glCreateShader(stage);
        glShaderSource(glShader, 1, &source, nullptr);
        glCompileShader(glShader);

        GLint compileStatus = GL_FALSE;
        glGetShaderiv(glShader, GL_COMPILE_STATUS, &compileStatus);
        if (compileStatus == GL_FALSE)
        {
            char log[2048] = { 0 };
            glGetShaderInfoLog(glShader, sizeof(log) - 1, nullptr, log);
            CCPrint(PrintManager::CHANNEL_WARN, "Gfx::CreateShader %s compile failed (%s):\n%s",
                    GlShaderStageName(stage),
                    debugName ? debugName : "<unnamed>", log);
            glDeleteShader(glShader);
            glShader = 0;
        }
        return glShader;
    }

    static bool LinkGlProgram(GLuint program, const char* debugName)
    {
        glLinkProgram(program);

        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        bool result = (linkStatus == GL_TRUE);
        if (!result)
        {
            char log[2048] = { 0 };
            glGetProgramInfoLog(program, sizeof(log) - 1, nullptr, log);
            CCPrint(PrintManager::CHANNEL_WARN, "Gfx::CreateShader link failed (%s):\n%s",
                    debugName ? debugName : "<unnamed>", log);
        }
        return result;
    }

    ShaderHandle RenderApiGles::CreateShader(const ShaderDescription& description)
    {
        const bool isCompute  = description.computeSource != nullptr;
        const bool isGraphics = description.vertexSource != nullptr && description.fragmentSource != nullptr;
        CC_ASSERT(isCompute ^ isGraphics, "ShaderDescription must supply either compute or vertex+fragment sources");

        GlShader entry;
        entry.isCompute = isCompute;
        entry.isAlive   = true;
        entry.program   = glCreateProgram();

        bool compileOk = true;

        if (isCompute)
        {
            GLuint cs = CompileGlShaderStage(GL_COMPUTE_SHADER, description.computeSource, description.debugName);
            if (cs == 0)
            {
                compileOk = false;
            }
            else
            {
                glAttachShader(entry.program, cs);
                compileOk = LinkGlProgram(entry.program, description.debugName);
                glDetachShader(entry.program, cs);
                glDeleteShader(cs);
            }
        }
        else
        {
            GLuint vs = CompileGlShaderStage(GL_VERTEX_SHADER, description.vertexSource, description.debugName);
            GLuint fs = CompileGlShaderStage(GL_FRAGMENT_SHADER, description.fragmentSource, description.debugName);
            if (vs == 0 || fs == 0)
            {
                compileOk = false;
            }
            else
            {
                glAttachShader(entry.program, vs);
                glAttachShader(entry.program, fs);
                compileOk = LinkGlProgram(entry.program, description.debugName);
                glDetachShader(entry.program, vs);
                glDetachShader(entry.program, fs);
            }
            if (vs != 0) glDeleteShader(vs);
            if (fs != 0) glDeleteShader(fs);
        }

        if (!compileOk)
        {
            glDeleteProgram(entry.program);
            entry.program = 0;
            entry.isAlive = false;
            ShaderHandle invalid;
            return invalid;
        }

        uint32_t slotIndex = GlCommon::AcquireSlot(freeShaderSlots, static_cast<uint32_t>(shaders.size()));
        if (slotIndex == shaders.size())
        {
            shaders.push_back(entry);
        }
        else
        {
            shaders[slotIndex] = entry;
        }

        ShaderHandle handle;
        handle.id = slotIndex;
        return handle;
    }

    void RenderApiGles::DestroyShader(ShaderHandle handle)
    {
        if (handle.IsValid())
        {
            uint32_t slotIndex = handle.id;
            CC_ASSERT(slotIndex < shaders.size(), "ShaderHandle out of range");
            GlShader& entry = shaders[slotIndex];
            if (entry.isAlive)
            {
                glDeleteProgram(entry.program);
                entry = GlShader();
                freeShaderSlots.push_back(slotIndex);
            }
        }
    }

    // ========================
    // Pipelines
    // ========================

    static bool IsNormalizedAttribType(VertexAttribType type)
    {
        return type == VertexAttribType::Uint8Norm;
    }

    PipelineHandle RenderApiGles::CreatePipeline(const PipelineDescription& description)
    {
        CC_ASSERT(description.shader.IsValid(), "PipelineDescription: shader handle is invalid");
        uint32_t shaderSlot = description.shader.id;
        CC_ASSERT(shaderSlot < shaders.size() && shaders[shaderSlot].isAlive, "PipelineDescription: shader not alive");

        GlPipeline entry;
        entry.description = description;
        entry.isAlive     = true;

        const bool isCompute = shaders[shaderSlot].isCompute;
        if (!isCompute)
        {
            glGenVertexArrays(1, &entry.vao);
            glBindVertexArray(entry.vao);

            for (int attribIndex = 0; attribIndex < description.vertexLayout.attributeCount; attribIndex++)
            {
                const VertexAttribute& attribute = description.vertexLayout.attributes[attribIndex];
                glEnableVertexAttribArray(attribute.location);
                glVertexAttribFormat(attribute.location,
                                     attribute.components,
                                     ToGlVertexAttribType(attribute.type),
                                     IsNormalizedAttribType(attribute.type) ? GL_TRUE : GL_FALSE,
                                     static_cast<GLuint>(attribute.offsetBytes));
                glVertexAttribBinding(attribute.location, 0);
            }

            glBindVertexArray(0);
        }

        uint32_t slotIndex = GlCommon::AcquireSlot(freePipelineSlots, static_cast<uint32_t>(pipelines.size()));
        if (slotIndex == pipelines.size())
        {
            pipelines.push_back(entry);
        }
        else
        {
            pipelines[slotIndex] = entry;
        }

        PipelineHandle handle;
        handle.id = slotIndex;
        return handle;
    }

    void RenderApiGles::DestroyPipeline(PipelineHandle handle)
    {
        if (handle.IsValid())
        {
            uint32_t slotIndex = handle.id;
            CC_ASSERT(slotIndex < pipelines.size(), "PipelineHandle out of range");
            GlPipeline& entry = pipelines[slotIndex];
            if (entry.isAlive)
            {
                if (entry.vao != 0)
                {
                    glDeleteVertexArrays(1, &entry.vao);
                }
                entry = GlPipeline();
                freePipelineSlots.push_back(slotIndex);
            }
            if (currentPipeline.id == slotIndex)
            {
                currentPipeline = PipelineHandle();
            }
        }
    }

    // ========================
    // Render targets
    // ========================

    RenderTargetHandle RenderApiGles::CreateRenderTarget(const RenderTargetDescription& description)
    {
        CC_ASSERT(description.colorAttachmentCount >= 0 && description.colorAttachmentCount <= MAX_COLOR_ATTACHMENTS,
                  "CreateRenderTarget: colorAttachmentCount out of range");

        GlRenderTarget entry;
        entry.description = description;
        entry.width       = description.width;
        entry.height      = description.height;
        entry.isAlive     = true;

        glGenFramebuffers(1, &entry.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, entry.fbo);

        GLenum drawBuffers[MAX_COLOR_ATTACHMENTS];
        for (int i = 0; i < description.colorAttachmentCount; i++)
        {
            const RenderTargetAttachment& attachment = description.colorAttachments[i];
            CC_ASSERT(attachment.arrayLayer == 0, "CreateRenderTarget: array-layer attachments not supported yet");
            CC_ASSERT(attachment.texture.IsValid(), "CreateRenderTarget: colour attachment texture is invalid");
            uint32_t texSlot = attachment.texture.id;
            CC_ASSERT(texSlot < textures.size() && textures[texSlot].isAlive,
                      "CreateRenderTarget: colour attachment texture not alive");
            GlTexture& tex = textures[texSlot];

            // GLES 3.1: sized colour-renderable formats only. R8/RG8/RGBA8
            // and the *16F formats used by engine render targets qualify;
            // a WebGL backend would still need EXT_color_buffer_float here.
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, tex.target,
                                   tex.glHandle, attachment.mipLevel);
            drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;

            if (entry.width == 0 || entry.height == 0)
            {
                entry.width  = tex.width;
                entry.height = tex.height;
            }
        }

        if (description.colorAttachmentCount > 0)
        {
            glDrawBuffers(description.colorAttachmentCount, drawBuffers);
        }
        else
        {
            GLenum none = GL_NONE;
            glDrawBuffers(1, &none);
        }

        if (description.hasDepthStencil)
        {
            const RenderTargetAttachment& depthAttachment = description.depthStencilAttachment;
            CC_ASSERT(depthAttachment.texture.IsValid(), "CreateRenderTarget: depth attachment texture is invalid");
            uint32_t depthSlot = depthAttachment.texture.id;
            CC_ASSERT(depthSlot < textures.size() && textures[depthSlot].isAlive,
                      "CreateRenderTarget: depth attachment texture not alive");
            GlTexture& depthTex = textures[depthSlot];
            GlTextureFormatInfo depthInfo = ToGlTextureFormat(depthTex.format);
            GLenum attachPoint = (depthInfo.pixelFormat == GL_DEPTH_STENCIL)
                ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachPoint, depthTex.target,
                                   depthTex.glHandle, depthAttachment.mipLevel);

            if (entry.width == 0 || entry.height == 0)
            {
                entry.width  = depthTex.width;
                entry.height = depthTex.height;
            }
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        CC_ASSERT(status == GL_FRAMEBUFFER_COMPLETE, "CreateRenderTarget: framebuffer incomplete");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        InvalidateCachedState();

        uint32_t slotIndex = GlCommon::AcquireSlot(freeRenderTargetSlots, static_cast<uint32_t>(renderTargets.size()));
        if (slotIndex == renderTargets.size())
        {
            renderTargets.push_back(entry);
        }
        else
        {
            renderTargets[slotIndex] = entry;
        }

        RenderTargetHandle handle;
        handle.id = slotIndex;
        return handle;
    }

    void RenderApiGles::DestroyRenderTarget(RenderTargetHandle handle)
    {
        if (handle.IsValid())
        {
            uint32_t slotIndex = handle.id;
            CC_ASSERT(slotIndex < renderTargets.size(), "RenderTargetHandle out of range");
            GlRenderTarget& entry = renderTargets[slotIndex];
            if (entry.isAlive)
            {
                if (currentRenderTarget.id == slotIndex)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    currentRenderTarget    = RenderTargetHandle();
                    currentPassIsOffscreen = false;
                    InvalidateCachedState();
                }
                // Attachment textures are owned by the caller and left intact.
                glDeleteFramebuffers(1, &entry.fbo);
                entry = GlRenderTarget();
                freeRenderTargetSlots.push_back(slotIndex);
            }
        }
    }

    // ========================
    // Command recording
    // ========================

    void RenderApiGles::BindPipeline(PipelineHandle pipeline)
    {
        CC_ASSERT(pipeline.IsValid(), "BindPipeline: invalid handle");

        bool alreadyBound = (pipeline.id == currentPipeline.id && currentPipeline.IsValid());
        if (!alreadyBound)
        {
            uint32_t slotIndex = pipeline.id;
            CC_ASSERT(slotIndex < pipelines.size(), "PipelineHandle out of range");
            GlPipeline& entry = pipelines[slotIndex];
            CC_ASSERT(entry.isAlive, "BindPipeline: pipeline destroyed");

            const PipelineDescription& desc = entry.description;

            uint32_t shaderSlot = desc.shader.id;
            glUseProgram(shaders[shaderSlot].program);

            if (entry.vao != 0)
            {
                glBindVertexArray(entry.vao);
            }

            if (desc.rasterizer.cullMode == CullMode::None)
            {
                glDisable(GL_CULL_FACE);
            }
            else
            {
                glEnable(GL_CULL_FACE);
                glCullFace(ToGlCullFace(desc.rasterizer.cullMode));
            }
            glFrontFace(desc.rasterizer.frontFace == FrontFace::Clockwise ? GL_CW : GL_CCW);

            if (desc.depthStencil.depthTestEnabled)
            {
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(ToGlCompareOp(desc.depthStencil.depthCompare));
            }
            else
            {
                glDisable(GL_DEPTH_TEST);
            }
            glDepthMask(desc.depthStencil.depthWriteEnabled ? GL_TRUE : GL_FALSE);

            if (desc.blend.enabled)
            {
                glEnable(GL_BLEND);
                glBlendFuncSeparate(ToGlBlendFactor(desc.blend.srcColorFactor),
                                    ToGlBlendFactor(desc.blend.dstColorFactor),
                                    ToGlBlendFactor(desc.blend.srcAlphaFactor),
                                    ToGlBlendFactor(desc.blend.dstAlphaFactor));
                glBlendEquationSeparate(ToGlBlendOp(desc.blend.colorOp), ToGlBlendOp(desc.blend.alphaOp));
            }
            else
            {
                glDisable(GL_BLEND);
            }

            currentPipeline = pipeline;
        }
    }

    void RenderApiGles::BindVertexBuffer(int slot, BufferHandle buffer, int offsetBytes, int strideBytes)
    {
        CC_ASSERT(buffer.IsValid(), "BindVertexBuffer: invalid handle");
        uint32_t slotIndex = buffer.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "BindVertexBuffer: buffer destroyed");

        glBindVertexBuffer(slot, entry.glHandle, offsetBytes, strideBytes);
    }

    void RenderApiGles::BindIndexBuffer(BufferHandle buffer, IndexType type)
    {
        CC_ASSERT(buffer.IsValid(), "BindIndexBuffer: invalid handle");
        uint32_t slotIndex = buffer.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "BindIndexBuffer: buffer destroyed");

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry.glHandle);
        currentIndexType = ToGlIndexType(type);
    }

    void RenderApiGles::BindTexture(int slot, TextureHandle texture, SamplerHandle sampler)
    {
        CC_ASSERT(texture.IsValid(), "BindTexture: invalid texture handle");
        uint32_t textureSlot = texture.id;
        CC_ASSERT(textureSlot < textures.size(), "TextureHandle out of range");
        GlTexture& texEntry = textures[textureSlot];
        CC_ASSERT(texEntry.isAlive, "BindTexture: texture destroyed");

        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(texEntry.target, texEntry.glHandle);

        if (sampler.IsValid())
        {
            uint32_t samplerSlot = sampler.id;
            CC_ASSERT(samplerSlot < samplers.size(), "SamplerHandle out of range");
            GlSampler& samplerEntry = samplers[samplerSlot];
            CC_ASSERT(samplerEntry.isAlive, "BindTexture: sampler destroyed");
            glBindSampler(slot, samplerEntry.glHandle);
        }
        else
        {
            glBindSampler(slot, 0);
        }
    }

    void RenderApiGles::BindUniformBuffer(int slot, BufferHandle buffer, int offsetBytes, int sizeBytes)
    {
        CC_ASSERT(buffer.IsValid(), "BindUniformBuffer: invalid handle");
        uint32_t slotIndex = buffer.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "BindUniformBuffer: buffer destroyed");

        glBindBufferRange(GL_UNIFORM_BUFFER, slot, entry.glHandle, offsetBytes, sizeBytes);
    }

    void RenderApiGles::BindStorageBuffer(int slot, BufferHandle buffer)
    {
        CC_ASSERT(buffer.IsValid(), "BindStorageBuffer: invalid handle");
        uint32_t slotIndex = buffer.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "BindStorageBuffer: buffer destroyed");

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, entry.glHandle);
    }

    void RenderApiGles::BindImage(int slot, TextureHandle texture, int mipLevel, ImageAccess access, TextureFormat format)
    {
        (void)slot; (void)texture; (void)mipLevel; (void)access; (void)format;
        CC_ASSERT(false, "BindImage not yet implemented in GLES backend");
    }

    void RenderApiGles::SetPushConstants(const void* data, int sizeBytes)
    {
        CC_ASSERT(data != nullptr, "SetPushConstants: null data");
        CC_ASSERT(sizeBytes > 0 && sizeBytes <= OBJECT_UNIFORMS_SIZE_BYTES,
            "SetPushConstants: size out of range");

        if (pushConstantsUbo == 0)
        {
            glGenBuffers(1, &pushConstantsUbo);
            glBindBuffer(GL_UNIFORM_BUFFER, pushConstantsUbo);
            glBufferData(GL_UNIFORM_BUFFER, OBJECT_UNIFORMS_SIZE_BYTES, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER, OBJECT_UNIFORMS_BINDING_SLOT, pushConstantsUbo);
        }
        else
        {
            glBindBuffer(GL_UNIFORM_BUFFER, pushConstantsUbo);
        }
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeBytes, data);
    }

    void RenderApiGles::Draw(int vertexCount, int instanceCount, int firstVertex)
    {
        CC_ASSERT(currentPipeline.IsValid(), "Draw: no pipeline bound");
        uint32_t slotIndex = currentPipeline.id;
        GLenum topology = ToGlPrimitiveTopology(pipelines[slotIndex].description.topology);

        if (instanceCount <= 1)
        {
            glDrawArrays(topology, firstVertex, vertexCount);
        }
        else
        {
            glDrawArraysInstanced(topology, firstVertex, vertexCount, instanceCount);
        }
    }

    void RenderApiGles::DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset)
    {
        // glDrawElementsBaseVertex requires GLES 3.2; v1 paths supply
        // vertexOffset == 0 so the base GLES 3.1 entry points suffice.
        CC_ASSERT(vertexOffset == 0, "DrawIndexed: nonzero vertexOffset requires GLES 3.2");
        CC_ASSERT(currentPipeline.IsValid(), "DrawIndexed: no pipeline bound");
        uint32_t slotIndex = currentPipeline.id;
        GLenum topology = ToGlPrimitiveTopology(pipelines[slotIndex].description.topology);

        const int indexSizeBytes = (currentIndexType == GL_UNSIGNED_SHORT) ? 2 : 4;
        const void* indexOffset = reinterpret_cast<const void*>(static_cast<uintptr_t>(firstIndex * indexSizeBytes));

        if (instanceCount <= 1)
        {
            glDrawElements(topology, indexCount, currentIndexType, indexOffset);
        }
        else
        {
            glDrawElementsInstanced(topology, indexCount, currentIndexType, indexOffset, instanceCount);
        }
    }

    void RenderApiGles::DrawIndirect(BufferHandle argsBuffer, int offsetBytes)
    {
        (void)argsBuffer; (void)offsetBytes;
        CC_ASSERT(false, "DrawIndirect not yet implemented in GLES backend");
    }

    // ========================
    // Compute
    // ========================

    void RenderApiGles::DispatchCompute(int groupsX, int groupsY, int groupsZ)
    {
        glDispatchCompute(groupsX, groupsY, groupsZ);
    }

    void RenderApiGles::DispatchComputeIndirect(BufferHandle argsBuffer, int offsetBytes)
    {
        CC_ASSERT(argsBuffer.IsValid(), "DispatchComputeIndirect: invalid args buffer");
        uint32_t slotIndex = argsBuffer.id;
        CC_ASSERT(slotIndex < buffers.size(), "DispatchComputeIndirect: buffer handle out of range");
        GlBuffer& entry = buffers[slotIndex];
        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, entry.glHandle);
        glDispatchComputeIndirect(static_cast<GLintptr>(offsetBytes));
        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
    }

    // ========================
    // Synchronisation
    // ========================

    void RenderApiGles::MemoryBarrier(unsigned int barrierBits)
    {
        glMemoryBarrier(ToGlBarrierBits(barrierBits));
    }

    void RenderApiGles::InvalidateCachedState()
    {
        currentPipeline = PipelineHandle();
    }
}
