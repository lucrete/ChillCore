#include "GfxRenderApiOpenGl.h"
#include "GlHandlePool.h"
#include <GLFW/glfw3.h>
#include "CCAssert.h"
#include "PrintManager.h"
#include "ObjectUniforms.h"
#include "FrameBufferOpenGl.h"
#include "QuadMesh.h"
#include "PlatformWindow.h"
#include "ShaderManager.h"

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
            case BufferUsage::Vertex:   result = GL_ARRAY_BUFFER;         break;
            case BufferUsage::Index:    result = GL_ELEMENT_ARRAY_BUFFER; break;
            case BufferUsage::Uniform:  result = GL_UNIFORM_BUFFER;       break;
            case BufferUsage::Storage:  result = GL_SHADER_STORAGE_BUFFER; break;
            case BufferUsage::Indirect: result = GL_DRAW_INDIRECT_BUFFER; break;
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
        GlTextureFormatInfo result = { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false };
        switch (format)
        {
            case TextureFormat::R8Unorm:         result = { GL_R8,               GL_RED,             GL_UNSIGNED_BYTE, false, false }; break;
            case TextureFormat::Rg8Unorm:        result = { GL_RG8,              GL_RG,              GL_UNSIGNED_BYTE, false, false }; break;
            case TextureFormat::Rgba8Unorm:      result = { GL_RGBA8,            GL_RGBA,            GL_UNSIGNED_BYTE, false, false }; break;
            case TextureFormat::Rgba8Srgb:       result = { GL_SRGB8_ALPHA8,     GL_RGBA,            GL_UNSIGNED_BYTE, false, false }; break;
            case TextureFormat::Rgb10A2Unorm:    result = { GL_RGB10_A2,         GL_RGBA,            GL_UNSIGNED_INT_2_10_10_10_REV, false, false }; break;
            case TextureFormat::R16Float:        result = { GL_R16F,             GL_RED,             GL_HALF_FLOAT, false, false }; break;
            case TextureFormat::Rg16Float:       result = { GL_RG16F,            GL_RG,              GL_HALF_FLOAT, false, false }; break;
            case TextureFormat::Rgba16Float:     result = { GL_RGBA16F,          GL_RGBA,            GL_HALF_FLOAT, false, false }; break;
            case TextureFormat::R32Float:        result = { GL_R32F,             GL_RED,             GL_FLOAT, false, false }; break;
            case TextureFormat::Rgba32Float:     result = { GL_RGBA32F,          GL_RGBA,            GL_FLOAT, false, false }; break;
            case TextureFormat::Bc1:             result = { GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_RGBA, GL_UNSIGNED_BYTE, true,  false }; break;
            case TextureFormat::Bc3:             result = { GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_RGBA, GL_UNSIGNED_BYTE, true,  false }; break;
            case TextureFormat::Bc7:             result = { GL_COMPRESSED_RGBA_BPTC_UNORM,    GL_RGBA, GL_UNSIGNED_BYTE, true,  false }; break;
            case TextureFormat::Depth24Stencil8: result = { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8, false, true }; break;
            case TextureFormat::Depth32Float:    result = { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT,          false, true }; break;
            default:
                CC_ASSERT(false, "Unsupported TextureFormat for GL");
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
        GLenum result = GL_REPEAT;
        switch (mode)
        {
            case AddressMode::Repeat:         result = GL_REPEAT; break;
            case AddressMode::MirroredRepeat: result = GL_MIRRORED_REPEAT; break;
            case AddressMode::ClampToEdge:    result = GL_CLAMP_TO_EDGE; break;
            case AddressMode::ClampToBorder:  result = GL_CLAMP_TO_BORDER; break;
            default: CC_ASSERT(false, "Unknown AddressMode"); break;
        }
        return result;
    }

    static GLenum ToGlCompareOp(CompareOp op)
    {
        GLenum result = GL_LESS;
        switch (op)
        {
            case CompareOp::Never:        result = GL_NEVER; break;
            case CompareOp::Less:         result = GL_LESS; break;
            case CompareOp::Equal:        result = GL_EQUAL; break;
            case CompareOp::LessEqual:    result = GL_LEQUAL; break;
            case CompareOp::Greater:      result = GL_GREATER; break;
            case CompareOp::NotEqual:     result = GL_NOTEQUAL; break;
            case CompareOp::GreaterEqual: result = GL_GEQUAL; break;
            case CompareOp::Always:       result = GL_ALWAYS; break;
            default: CC_ASSERT(false, "Unknown CompareOp"); break;
        }
        return result;
    }

    static GLenum ToGlBlendFactor(BlendFactor factor)
    {
        GLenum result = GL_ONE;
        switch (factor)
        {
            case BlendFactor::Zero:             result = GL_ZERO; break;
            case BlendFactor::One:              result = GL_ONE; break;
            case BlendFactor::SrcColor:         result = GL_SRC_COLOR; break;
            case BlendFactor::OneMinusSrcColor: result = GL_ONE_MINUS_SRC_COLOR; break;
            case BlendFactor::DstColor:         result = GL_DST_COLOR; break;
            case BlendFactor::OneMinusDstColor: result = GL_ONE_MINUS_DST_COLOR; break;
            case BlendFactor::SrcAlpha:         result = GL_SRC_ALPHA; break;
            case BlendFactor::OneMinusSrcAlpha: result = GL_ONE_MINUS_SRC_ALPHA; break;
            case BlendFactor::DstAlpha:         result = GL_DST_ALPHA; break;
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
            case BlendOp::Add:             result = GL_FUNC_ADD; break;
            case BlendOp::Subtract:        result = GL_FUNC_SUBTRACT; break;
            case BlendOp::ReverseSubtract: result = GL_FUNC_REVERSE_SUBTRACT; break;
            case BlendOp::Min:             result = GL_MIN; break;
            case BlendOp::Max:             result = GL_MAX; break;
            default: CC_ASSERT(false, "Unknown BlendOp"); break;
        }
        return result;
    }

    static GLenum ToGlPrimitiveTopology(PrimitiveTopology topology)
    {
        GLenum result = GL_TRIANGLES;
        switch (topology)
        {
            case PrimitiveTopology::Points:        result = GL_POINTS; break;
            case PrimitiveTopology::Lines:         result = GL_LINES; break;
            case PrimitiveTopology::LineStrip:     result = GL_LINE_STRIP; break;
            case PrimitiveTopology::Triangles:     result = GL_TRIANGLES; break;
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
            case VertexAttribType::Float32:   result = GL_FLOAT; break;
            case VertexAttribType::Float16:   result = GL_HALF_FLOAT; break;
            case VertexAttribType::Uint8Norm: result = GL_UNSIGNED_BYTE; break;
            case VertexAttribType::Uint8:     result = GL_UNSIGNED_BYTE; break;
            case VertexAttribType::Uint16:    result = GL_UNSIGNED_SHORT; break;
            case VertexAttribType::Uint32:    result = GL_UNSIGNED_INT; break;
            case VertexAttribType::Int32:     result = GL_INT; break;
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

    RenderApiOpenGl::RenderApiOpenGl()
    {
    }

    RenderApiOpenGl::~RenderApiOpenGl()
    {
    }

    static void GLAD_API_PTR GlDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                              GLsizei length, const GLchar* message, const void* userParam)
    {
        (void)source; (void)id; (void)length; (void)userParam;
        const char* sevName = "?";
        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:         sevName = "HIGH"; break;
            case GL_DEBUG_SEVERITY_MEDIUM:       sevName = "MED";  break;
            case GL_DEBUG_SEVERITY_LOW:          sevName = "LOW";  break;
            case GL_DEBUG_SEVERITY_NOTIFICATION: sevName = "INFO"; break;
            default: break;
        }
        const char* typeName = "?";
        switch (type)
        {
            case GL_DEBUG_TYPE_ERROR:               typeName = "ERROR"; break;
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeName = "DEPRECATED"; break;
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeName = "UNDEFINED"; break;
            case GL_DEBUG_TYPE_PORTABILITY:         typeName = "PORTABILITY"; break;
            case GL_DEBUG_TYPE_PERFORMANCE:         typeName = "PERF"; break;
            case GL_DEBUG_TYPE_MARKER:              typeName = "MARKER"; break;
            case GL_DEBUG_TYPE_OTHER:               typeName = "OTHER"; break;
            default: break;
        }
        // Report MEDIUM and HIGH severity — ERRORS and real correctness issues.
        // LOW and NOTIFICATION are driver chatter (performance hints, buffer
        // allocation notes); filtered out to keep the console quiet.
        if (severity == GL_DEBUG_SEVERITY_MEDIUM || severity == GL_DEBUG_SEVERITY_HIGH)
        {
            CCPrint(PrintManager::CHANNEL_RENDER, "GL[%s/%s]: %s", sevName, typeName, message);
        }
    }

    void RenderApiOpenGl::Init()
    {
        // PlatformWindow has already created the window + GL context.
        // This backend is desktop-GLFW-tied; load GL via glfwGetProcAddress
        // directly. The Android GL ES backend will be a sibling file that
        // calls eglGetProcAddress.
        gladLoadGL(glfwGetProcAddress);

        QueryCapabilities();

        // Enable GL debug callback to surface driver warnings that don't show
        // up via glGetError. The callback itself filters by severity.
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GlDebugCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }

    void RenderApiOpenGl::Shutdown()
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

        textures.clear();
        samplers.clear();
        renderTargets.clear();
        freeTextureSlots.clear();
        freeSamplerSlots.clear();
        freeRenderTargetSlots.clear();

        if (pushConstantsUbo != 0)
        {
            glDeleteBuffers(1, &pushConstantsUbo);
            pushConstantsUbo = 0;
        }

        if (sceneFrameBuffer != nullptr)
        {
            delete sceneFrameBuffer;
            sceneFrameBuffer = nullptr;
        }

        for (int f = 0; f < GlCommon::FRAMES_IN_FLIGHT; f++)
        {
            GlCommon::GpuScopeFrame& frame = scopeTimer.frames[f];
            for (int s = 0; s < GlCommon::MAX_SCOPES_PER_FRAME; s++)
            {
                if (frame.scopes[s].queryBegin != 0) { glDeleteQueries(1, &frame.scopes[s].queryBegin); }
                if (frame.scopes[s].queryEnd   != 0) { glDeleteQueries(1, &frame.scopes[s].queryEnd); }
            }
            frame = GlCommon::GpuScopeFrame();
        }
        scopeTimer.resolvedScopeCount  = 0;
        scopeTimer.lastFrameDurationMs = 0.0f;
    }

    const GfxCapabilities& RenderApiOpenGl::GetCapabilities() const
    {
        return capabilities;
    }

    void RenderApiOpenGl::QueryCapabilities()
    {
        GLint maxTextureSize = 0;
        GLint maxVertexAttribs = 0;
        GLint maxUniformBlockSize = 0;
        GLint maxSsboSize = 0;
        GLint maxColorAttachments = 0;
        GLint maxSamples = 0;
        GLint maxComputeX = 0;
        GLint maxComputeY = 0;
        GLint maxComputeZ = 0;
        GLint maxComputeInvocations = 0;

        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxComputeX);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxComputeY);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxComputeZ);
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxComputeInvocations);
        glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxSsboSize);

        capabilities.supportsComputeShaders        = true;
        capabilities.supportsStorageBuffers        = true;
        capabilities.supportsIndirectDraw          = true;
        capabilities.supportsGeometryShader        = true;
        capabilities.supportsTessellation          = true;
        capabilities.supportsBcTextureFormats      = true;
        capabilities.supportsAstcTextureFormats    = false;
        capabilities.supportsAnisotropicFiltering  = true;
        capabilities.supportsDebugMarkers          = true;
        capabilities.supportsGpuTimestamps         = (GLAD_GL_ARB_timer_query != 0);

        capabilities.maxTextureSize                = maxTextureSize;
        capabilities.maxVertexAttributes           = maxVertexAttribs;
        capabilities.maxUniformBufferSizeBytes     = maxUniformBlockSize;
        capabilities.maxStorageBufferSizeBytes     = maxSsboSize;
        capabilities.maxColorAttachments           = maxColorAttachments;
        capabilities.maxMsaaSamples                = maxSamples;
        capabilities.maxComputeWorkgroupSizeX      = maxComputeX;
        capabilities.maxComputeWorkgroupSizeY      = maxComputeY;
        capabilities.maxComputeWorkgroupSizeZ      = maxComputeZ;
        capabilities.maxComputeWorkgroupInvocations = maxComputeInvocations;

        capabilities.backendName   = "OpenGL";
        capabilities.deviceName    = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        capabilities.driverVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    }

    // ========================
    // Frame
    // ========================

    void RenderApiOpenGl::BeginFrame()
    {
        // No-op on GL. Other backends may acquire the next swapchain image
        // here (Vulkan vkAcquireNextImageKHR, D3D12 GetCurrentBackBufferIndex).
    }

    void RenderApiOpenGl::EndFrame()
    {
        // Open a "GpuRenderEnd" scope that spans the swap / vsync wait.
        // AdvanceScopeRing closes it after SwapBuffers returns.
        AddGpuTimestamp("GpuRenderEnd");
        CC::PlatformWindow::Get()->SwapBuffers();
        AdvanceScopeRing();
    }

    void RenderApiOpenGl::AdvanceScopeRing()
    {
        // Close the outgoing frame's still-open scope (GpuRenderEnd). Its
        // duration captures the swap + vsync wait that just completed.
        GlCommon::GpuScopeFrame& outgoing = scopeTimer.frames[scopeTimer.currentFrameIndex];
        if (outgoing.currentOpenScope >= 0 && capabilities.supportsGpuTimestamps)
        {
            GlCommon::GpuScope& scope = outgoing.scopes[outgoing.currentOpenScope];
            glQueryCounter(scope.queryEnd, GL_TIMESTAMP);
            if (capabilities.supportsDebugMarkers)
            {
                glPopDebugGroup();
            }
            outgoing.currentOpenScope = -1;
        }

        // Rotate ring. The slot we're about to write into is the one whose
        // readable results are oldest; resolve it before overwrite.
        scopeTimer.currentFrameIndex = (scopeTimer.currentFrameIndex + 1) % GlCommon::FRAMES_IN_FLIGHT;
        GlCommon::GpuScopeFrame& frame = scopeTimer.frames[scopeTimer.currentFrameIndex];

        if (frame.everUsed)
        {
            ResolveFrameScopes(frame);
        }

        frame.scopeCount       = 0;
        frame.currentOpenScope = -1;

        // Open the Idle scope on the new ring slot. Closes on the next
        // "GpuRenderBegin" timestamp emitted by BeginRenderPass.
        AddGpuTimestamp("GpuFrameBegin");
    }

    // ========================
    // Backbuffer / swapchain
    // ========================

    void RenderApiOpenGl::ConfigureBackbuffer(const BackbufferDescription& description)
    {
        backbufferDescription = description;

        // MSAA sample count of 1 means "disabled"; the FrameBufferOpenGl
        // API uses 0 for the non-MSAA case, so translate.
        int glSamples = (description.sampleCount > 1) ? description.sampleCount : 0;

        // First call creates the scene FB; later calls update sample count
        // and (eventually) format / size.
        if (sceneFrameBuffer == nullptr)
        {
            int width  = description.width;
            int height = description.height;
            if (width <= 0 || height <= 0)
            {
                CC::PlatformWindow::Get()->GetFramebufferSize(width, height);
            }
            sceneFrameBuffer = new CC::FrameBufferOpenGl(width, height, glSamples);
        }
        else
        {
            sceneFrameBuffer->SetMsaaSamples(glSamples);
            sceneFrameBuffer->SetMsaaEnabled(glSamples > 0);
        }
    }

    RenderTargetHandle RenderApiOpenGl::GetBackbuffer() const
    {
        // The default-framebuffer target isn't managed through the
        // RenderTarget pool yet. Callers that want to draw to the
        // backbuffer use BeginDefaultRenderPass. This accessor reserves a
        // seat for when render-pass handles subsume the concept.
        return RenderTargetHandle();
    }

    void RenderApiOpenGl::GetBackbufferSize(int& width, int& height) const
    {
        int outWidth  = backbufferDescription.width;
        int outHeight = backbufferDescription.height;
        if (outWidth <= 0 || outHeight <= 0)
        {
            // 0 in the descriptor means "match the window" — query the platform.
            CC::PlatformWindow::Get()->GetFramebufferSize(outWidth, outHeight);
        }
        width  = outWidth;
        height = outHeight;
    }

    // ========================
    // Render pass
    // ========================

    void RenderApiOpenGl::BeginDefaultRenderPass(const float clearColor[4], float clearDepth)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glClearDepth(static_cast<double>(clearDepth));
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void RenderApiOpenGl::BeginRenderPass(RenderTargetHandle target)
    {
        // The "backbuffer" target is currently the only supported value.
        // Future RenderTargetHandle pool entries (offscreen post-FX targets,
        // shadow maps, etc.) will branch here.
        (void)target;
        CC_ASSERT(sceneFrameBuffer != nullptr, "BeginRenderPass: scene FB not configured");
        CC_ASSERT(!renderPassActive, "BeginRenderPass: a render pass is already active");

        int width  = 0;
        int height = 0;
        GetBackbufferSize(width, height);

        sceneFrameBuffer->Bind(width, height);
        glEnable(GL_DEPTH_TEST);
        // glClear honours the depth write mask. A prior frame's transparent
        // pipeline may have left it disabled; ensure the depth clear lands.
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderPassActive = true;
        AddGpuTimestamp("GpuRenderBegin");
    }

    void RenderApiOpenGl::EnsureBlitPipeline()
    {
        if (blitPipeline.IsValid())
        {
            return;
        }

        BufferDescription vbDesc;
        vbDesc.sizeBytes   = sizeof(CC::QUAD_VERTICES);
        vbDesc.usage       = BufferUsage::Vertex;
        vbDesc.memory      = BufferMemory::GpuOnly;
        vbDesc.initialData = CC::QUAD_VERTICES;
        vbDesc.debugName   = "Gfx::Blit::Vertices";
        blitVertexBuffer = CreateBuffer(vbDesc);

        blitShader = CC::ShaderManager::Get()->GetShaderHandle("FullScreenBlit");

        VertexLayout layout;
        layout.strideBytes    = CC::QUAD_STRIDE_BYTES;
        layout.attributeCount = 2;
        layout.attributes[0]  = { 0, 0,                 VertexAttribType::Float32, 2 };
        layout.attributes[1]  = { 1, 2 * sizeof(float), VertexAttribType::Float32, 2 };

        PipelineDescription desc;
        desc.shader                         = blitShader;
        desc.vertexLayout                   = layout;
        desc.topology                       = PrimitiveTopology::Triangles;
        desc.rasterizer.cullMode            = CullMode::None;
        desc.rasterizer.frontFace           = FrontFace::CounterClockwise;
        desc.depthStencil.depthTestEnabled  = false;
        desc.depthStencil.depthWriteEnabled = false;
        desc.blend.enabled                  = false;
        blitPipeline = CreatePipeline(desc);
    }

    void RenderApiOpenGl::EndRenderPass()
    {
        CC_ASSERT(renderPassActive, "EndRenderPass: no render pass is active");
        CC_ASSERT(sceneFrameBuffer != nullptr, "EndRenderPass: scene FB not configured");

        // Resolve MSAA into the single-sample colorbuffer.
        sceneFrameBuffer->Resolve();
        AddGpuTimestamp("GpuAfterMsaaResolve");

        // Blit the resolved colorbuffer to the default framebuffer.
        EnsureBlitPipeline();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        BindPipeline(blitPipeline);
        BindVertexBuffer(0, blitVertexBuffer, 0, CC::QUAD_STRIDE_BYTES);

        // Bind the resolved colorbuffer at unit 0. The blit shader declares
        // layout(binding = 0). The colorbuffer is non-mipmapped — unbind
        // any sampler at unit 0 so the texture's own GL_LINEAR parameters
        // govern, otherwise a stale mipmap-needing sampler from an earlier
        // material draw silently produces black output.
        glActiveTexture(GL_TEXTURE0);
        sceneFrameBuffer->BindColorBuffer();
        glBindSampler(0, 0);

        Draw(CC::QUAD_VERTEX_COUNT, 1, 0);

        renderPassActive = false;
    }

    // ========================
    // Buffers
    // ========================

    BufferHandle RenderApiOpenGl::CreateBuffer(const BufferDescription& description)
    {
        CC_ASSERT(description.sizeBytes > 0, "BufferDescription.sizeBytes must be > 0");

        GlBuffer entry;
        entry.target     = ToGlBufferTarget(description.usage);
        entry.usageHint  = ToGlBufferUsageHint(description.memory);
        entry.sizeBytes  = description.sizeBytes;
        entry.usage      = description.usage;
        entry.memory     = description.memory;
        entry.isAlive    = true;

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

    void RenderApiOpenGl::DestroyBuffer(BufferHandle handle)
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

    void RenderApiOpenGl::UpdateBuffer(BufferHandle handle, int offsetBytes, int sizeBytes, const void* data)
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

    void RenderApiOpenGl::ReadBuffer(BufferHandle handle, int offsetBytes, int sizeBytes, void* destination)
    {
        CC_ASSERT(handle.IsValid(), "ReadBuffer: invalid handle");
        uint32_t slotIndex = handle.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "ReadBuffer: buffer already destroyed");

        glBindBuffer(entry.target, entry.glHandle);
        glGetBufferSubData(entry.target, offsetBytes, sizeBytes, destination);
        glBindBuffer(entry.target, 0);
    }

    // ========================
    // Textures
    // ========================

    TextureHandle RenderApiOpenGl::CreateTexture(const TextureDescription& description)
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

        // glTexStorage2D does NOT set TEXTURE_MAX_LEVEL; it stays at the default 1000.
        // Some drivers (observed on NVIDIA) treat the texture as mipmap-incomplete when
        // a sampler with a MIPMAP filter requests levels beyond the allocated range,
        // returning (0,0,0,0). Clamp to the allocated level count explicitly.
        glTexParameteri(entry.target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(entry.target, GL_TEXTURE_MAX_LEVEL, description.mipLevels - 1);

        if (description.initialData != nullptr && !formatInfo.isCompressed)
        {
            if (description.isCubemap)
            {
                const unsigned char* facePtr = static_cast<const unsigned char*>(description.initialData);
                int faceSizeBytes = description.width * description.height * 4;  // approximate; assumes 4 bpp
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

    void RenderApiOpenGl::DestroyTexture(TextureHandle handle)
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

    void RenderApiOpenGl::UpdateTexture(TextureHandle handle, int mipLevel, int x, int y, int width, int height, const void* data)
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

    SamplerHandle RenderApiOpenGl::CreateSampler(const SamplerDescription& description)
    {
        GlSampler entry;
        entry.isAlive = true;

        glGenSamplers(1, &entry.glHandle);
        glSamplerParameteri(entry.glHandle, GL_TEXTURE_MIN_FILTER, ToGlMinFilter(description.minFilter, description.mipmapMode));
        glSamplerParameteri(entry.glHandle, GL_TEXTURE_MAG_FILTER, ToGlMagFilter(description.magFilter));
        glSamplerParameteri(entry.glHandle, GL_TEXTURE_WRAP_S, ToGlAddressMode(description.addressU));
        glSamplerParameteri(entry.glHandle, GL_TEXTURE_WRAP_T, ToGlAddressMode(description.addressV));
        glSamplerParameteri(entry.glHandle, GL_TEXTURE_WRAP_R, ToGlAddressMode(description.addressW));

        if (capabilities.supportsAnisotropicFiltering && description.maxAnisotropy > 1.0f)
        {
            glSamplerParameterf(entry.glHandle, GL_TEXTURE_MAX_ANISOTROPY, description.maxAnisotropy);
        }

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

    void RenderApiOpenGl::DestroySampler(SamplerHandle handle)
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
            case GL_VERTEX_SHADER:          result = "Vertex";         break;
            case GL_FRAGMENT_SHADER:        result = "Fragment";       break;
            case GL_COMPUTE_SHADER:         result = "Compute";        break;
            case GL_GEOMETRY_SHADER:        result = "Geometry";       break;
            case GL_TESS_CONTROL_SHADER:    result = "TessControl";    break;
            case GL_TESS_EVALUATION_SHADER: result = "TessEvaluation"; break;
            default:                                                   break;
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

    ShaderHandle RenderApiOpenGl::CreateShader(const ShaderDescription& description)
    {
        const bool isCompute = description.computeSource != nullptr;
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

    void RenderApiOpenGl::DestroyShader(ShaderHandle handle)
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

    PipelineHandle RenderApiOpenGl::CreatePipeline(const PipelineDescription& description)
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

    void RenderApiOpenGl::DestroyPipeline(PipelineHandle handle)
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
    // Render targets (stubbed)
    // ========================

    RenderTargetHandle RenderApiOpenGl::CreateRenderTarget(const RenderTargetDescription& description)
    {
        (void)description;
        CC_ASSERT(false, "CreateRenderTarget not yet implemented in Phase 1a");
        return RenderTargetHandle();
    }

    void RenderApiOpenGl::DestroyRenderTarget(RenderTargetHandle handle)
    {
        (void)handle;
        CC_ASSERT(false, "DestroyRenderTarget not yet implemented in Phase 1a");
    }

    // ========================
    // Command recording (mostly stubbed)
    // ========================

    void RenderApiOpenGl::BindPipeline(PipelineHandle pipeline)
    {
        CC_ASSERT(pipeline.IsValid(), "BindPipeline: invalid handle");

        // Skip redundant binds. The opaque pass is sorted by pipeline so
        // consecutive draws of the same pipeline are common. Invalidated
        // on frame boundaries (StartFrame) and pipeline destruction.
        bool alreadyBound = (pipeline.id == currentPipeline.id && currentPipeline.IsValid());
        if (!alreadyBound)
        {
            uint32_t slotIndex = pipeline.id;
            CC_ASSERT(slotIndex < pipelines.size(), "PipelineHandle out of range");
            GlPipeline& entry = pipelines[slotIndex];
            CC_ASSERT(entry.isAlive, "BindPipeline: pipeline destroyed");

            const PipelineDescription& desc = entry.description;

            // Shader
            uint32_t shaderSlot = desc.shader.id;
            glUseProgram(shaders[shaderSlot].program);

            // VAO (graphics pipelines only)
            if (entry.vao != 0)
            {
                glBindVertexArray(entry.vao);
            }

            // Rasterizer
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

            // Depth
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

            // Blend
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

    void RenderApiOpenGl::BindVertexBuffer(int slot, BufferHandle buffer, int offsetBytes, int strideBytes)
    {
        CC_ASSERT(buffer.IsValid(), "BindVertexBuffer: invalid handle");
        uint32_t slotIndex = buffer.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "BindVertexBuffer: buffer destroyed");

        glBindVertexBuffer(slot, entry.glHandle, offsetBytes, strideBytes);
    }

    void RenderApiOpenGl::BindIndexBuffer(BufferHandle buffer, IndexType type)
    {
        CC_ASSERT(buffer.IsValid(), "BindIndexBuffer: invalid handle");
        uint32_t slotIndex = buffer.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "BindIndexBuffer: buffer destroyed");

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry.glHandle);
        currentIndexType = ToGlIndexType(type);
    }

    void RenderApiOpenGl::BindTexture(int slot, TextureHandle texture, SamplerHandle sampler)
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

    void RenderApiOpenGl::BindUniformBuffer(int slot, BufferHandle buffer, int offsetBytes, int sizeBytes)
    {
        CC_ASSERT(buffer.IsValid(), "BindUniformBuffer: invalid handle");
        uint32_t slotIndex = buffer.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "BindUniformBuffer: buffer destroyed");

        glBindBufferRange(GL_UNIFORM_BUFFER, slot, entry.glHandle, offsetBytes, sizeBytes);
    }

    void RenderApiOpenGl::BindStorageBuffer(int slot, BufferHandle buffer)
    {
        CC_ASSERT(buffer.IsValid(), "BindStorageBuffer: invalid handle");
        uint32_t slotIndex = buffer.id;
        CC_ASSERT(slotIndex < buffers.size(), "BufferHandle out of range");
        GlBuffer& entry = buffers[slotIndex];
        CC_ASSERT(entry.isAlive, "BindStorageBuffer: buffer destroyed");

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, entry.glHandle);
    }

    void RenderApiOpenGl::BindImage(int slot, TextureHandle texture, int mipLevel, ImageAccess access, TextureFormat format)
    {
        (void)slot; (void)texture; (void)mipLevel; (void)access; (void)format;
        CC_ASSERT(false, "BindImage not yet implemented in Phase 1a");
    }

    void RenderApiOpenGl::SetPushConstants(const void* data, int sizeBytes)
    {
        CC_ASSERT(data != nullptr, "SetPushConstants: null data");
        CC_ASSERT(sizeBytes > 0 && sizeBytes <= OBJECT_UNIFORMS_SIZE_BYTES,
            "SetPushConstants: size out of range");

        // Lazy create the single push-tier UBO. Bind it once at the push
        // slot; subsequent draws only need glBufferSubData.
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

    void RenderApiOpenGl::Draw(int vertexCount, int instanceCount, int firstVertex)
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

    void RenderApiOpenGl::DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset)
    {
        CC_ASSERT(currentPipeline.IsValid(), "DrawIndexed: no pipeline bound");
        uint32_t slotIndex = currentPipeline.id;
        GLenum topology = ToGlPrimitiveTopology(pipelines[slotIndex].description.topology);

        const int indexSizeBytes = (currentIndexType == GL_UNSIGNED_SHORT) ? 2 : 4;
        const void* indexOffset = reinterpret_cast<const void*>(static_cast<uintptr_t>(firstIndex * indexSizeBytes));

        if (instanceCount <= 1)
        {
            glDrawElementsBaseVertex(topology, indexCount, currentIndexType, indexOffset, vertexOffset);
        }
        else
        {
            glDrawElementsInstancedBaseVertex(topology, indexCount, currentIndexType,
                                              indexOffset, instanceCount, vertexOffset);
        }
    }

    void RenderApiOpenGl::DrawIndirect(BufferHandle argsBuffer, int offsetBytes)
    {
        (void)argsBuffer; (void)offsetBytes;
        CC_ASSERT(false, "DrawIndirect not yet implemented in Phase 1a");
    }

    // ========================
    // Compute
    // ========================

    void RenderApiOpenGl::DispatchCompute(int groupsX, int groupsY, int groupsZ)
    {
        CC_ASSERT(capabilities.supportsComputeShaders, "DispatchCompute: compute not supported on this backend");
        glDispatchCompute(groupsX, groupsY, groupsZ);
    }

    void RenderApiOpenGl::DispatchComputeIndirect(BufferHandle argsBuffer, int offsetBytes)
    {
        CC_ASSERT(capabilities.supportsComputeShaders, "DispatchComputeIndirect: compute not supported on this backend");
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

    void RenderApiOpenGl::MemoryBarrier(unsigned int barrierBits)
    {
        glMemoryBarrier(ToGlBarrierBits(barrierBits));
    }

    void RenderApiOpenGl::InvalidateCachedState()
    {
        currentPipeline = PipelineHandle();
    }

    // ========================
    // GPU scope timing
    // ========================

    void RenderApiOpenGl::AddGpuTimestamp(const char* name)
    {
        if (capabilities.supportsGpuTimestamps)
        {
            GlCommon::GpuScopeFrame& frame = scopeTimer.frames[scopeTimer.currentFrameIndex];

            // Implicitly close any scope currently open on this frame's slot.
            if (frame.currentOpenScope >= 0)
            {
                GlCommon::GpuScope& prev = frame.scopes[frame.currentOpenScope];
                glQueryCounter(prev.queryEnd, GL_TIMESTAMP);
                if (capabilities.supportsDebugMarkers)
                {
                    glPopDebugGroup();
                }
                frame.currentOpenScope = -1;
            }

            // Cap the number of scopes per frame. Extra timestamps beyond the
            // cap are dropped; callers should design their marker set to stay
            // within GlCommon::MAX_SCOPES_PER_FRAME.
            if (frame.scopeCount < GlCommon::MAX_SCOPES_PER_FRAME)
            {
                int slot = frame.scopeCount++;
                GlCommon::GpuScope& scope = frame.scopes[slot];

                // Names longer than the buffer are truncated rather than clipped silently.
                size_t copyLen = 0;
                if (name != nullptr)
                {
                    while (name[copyLen] != '\0' && copyLen < sizeof(scope.name) - 1)
                    {
                        scope.name[copyLen] = name[copyLen];
                        copyLen++;
                    }
                }
                scope.name[copyLen] = '\0';

                if (scope.queryBegin == 0)
                {
                    glGenQueries(1, &scope.queryBegin);
                    glGenQueries(1, &scope.queryEnd);
                }

                if (capabilities.supportsDebugMarkers)
                {
                    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, scope.name);
                }

                glQueryCounter(scope.queryBegin, GL_TIMESTAMP);
                frame.everUsed         = true;
                frame.currentOpenScope = slot;
            }
        }
    }

    void RenderApiOpenGl::ResolveFrameScopes(GlCommon::GpuScopeFrame& frame)
    {
        GLuint64 minBegin = 0;
        GLuint64 maxEnd   = 0;
        bool haveAny = false;

        scopeTimer.resolvedScopeCount = 0;

        for (int i = 0; i < frame.scopeCount; i++)
        {
            GlCommon::GpuScope& scope = frame.scopes[i];
            if (scope.queryBegin != 0 && scope.queryEnd != 0)
            {
                GLuint64 beginNs = 0;
                GLuint64 endNs   = 0;
                glGetQueryObjectui64v(scope.queryBegin, GL_QUERY_RESULT, &beginNs);
                glGetQueryObjectui64v(scope.queryEnd,   GL_QUERY_RESULT, &endNs);

                float durationMs = (endNs >= beginNs) ? static_cast<float>(endNs - beginNs) * 1.0e-6f : 0.0f;

                if (scopeTimer.resolvedScopeCount < GlCommon::MAX_SCOPES_PER_FRAME)
                {
                    GlCommon::ResolvedScope& out = scopeTimer.resolvedScopes[scopeTimer.resolvedScopeCount++];
                    size_t copyLen = 0;
                    while (scope.name[copyLen] != '\0' && copyLen < sizeof(out.name) - 1)
                    {
                        out.name[copyLen] = scope.name[copyLen];
                        copyLen++;
                    }
                    out.name[copyLen] = '\0';
                    out.durationMs    = durationMs;
                }

                if (!haveAny || beginNs < minBegin)
                {
                    minBegin = beginNs;
                    haveAny  = true;
                }
                if (endNs > maxEnd)
                {
                    maxEnd = endNs;
                }
            }
        }

        scopeTimer.lastFrameDurationMs = haveAny ? static_cast<float>(maxEnd - minBegin) * 1.0e-6f : 0.0f;
    }

    int RenderApiOpenGl::GetGpuScopeCount() const
    {
        return scopeTimer.resolvedScopeCount;
    }

    const char* RenderApiOpenGl::GetGpuScopeNameAt(int index) const
    {
        const char* result = "";
        if (index >= 0 && index < scopeTimer.resolvedScopeCount)
        {
            result = scopeTimer.resolvedScopes[index].name;
        }
        return result;
    }

    float RenderApiOpenGl::GetGpuScopeDurationMsAt(int index) const
    {
        float result = 0.0f;
        if (index >= 0 && index < scopeTimer.resolvedScopeCount)
        {
            result = scopeTimer.resolvedScopes[index].durationMs;
        }
        return result;
    }

    float RenderApiOpenGl::GetGpuFrameDurationMs() const
    {
        return scopeTimer.lastFrameDurationMs;
    }

    bool RenderApiOpenGl::IsGpuTimerSupported() const
    {
        return capabilities.supportsGpuTimestamps;
    }

    GLuint RenderApiOpenGl::GetGlShaderProgram(ShaderHandle handle) const
    {
        GLuint result = 0;
        if (handle.IsValid() && handle.id < shaders.size() && shaders[handle.id].isAlive)
        {
            result = shaders[handle.id].program;
        }
        return result;
    }

    // ========================
    // Shader program / uniforms (transitional)
    // ========================

    void RenderApiOpenGl::BindShaderProgram(ShaderHandle shader)
    {
        GLuint program = GetGlShaderProgram(shader);
        glUseProgram(program);
    }

    int RenderApiOpenGl::GetUniformLocation(ShaderHandle shader, const char* name)
    {
        int result = -1;
        GLuint program = GetGlShaderProgram(shader);
        if (program != 0 && name != nullptr)
        {
            result = glGetUniformLocation(program, name);
        }
        return result;
    }

    void RenderApiOpenGl::SetUniformMat4(int location, const float* values)
    {
        if (location != -1)
        {
            glUniformMatrix4fv(location, 1, GL_FALSE, values);
        }
    }

    void RenderApiOpenGl::SetUniformVec4(int location, const float* values)
    {
        if (location != -1)
        {
            glUniform4fv(location, 1, values);
        }
    }

    void RenderApiOpenGl::SetUniformVec3(int location, const float* values)
    {
        if (location != -1)
        {
            glUniform3fv(location, 1, values);
        }
    }

    void RenderApiOpenGl::SetUniformVec2(int location, const float* values)
    {
        if (location != -1)
        {
            glUniform2fv(location, 1, values);
        }
    }

    void RenderApiOpenGl::SetUniformFloat(int location, float value)
    {
        if (location != -1)
        {
            glUniform1f(location, value);
        }
    }

    void RenderApiOpenGl::SetUniformInt(int location, int value)
    {
        if (location != -1)
        {
            glUniform1i(location, value);
        }
    }
}
