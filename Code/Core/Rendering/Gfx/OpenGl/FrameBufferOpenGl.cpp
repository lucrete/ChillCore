#include "FrameBufferOpenGl.h"
#include <glad/gl.h>
#include "CCAssert.h"

namespace CC
{
    FrameBufferOpenGl::FrameBufferOpenGl(int initialWidth, int initialHeight, int msaaSamples)
        : width(initialWidth)
        , height(initialHeight)
        , samples(msaaSamples)
    {
        Create();
        if (samples > 0) 
        {
            CreateMsaa();
        }
    }

    FrameBufferOpenGl::~FrameBufferOpenGl()
    {
        Cleanup();
    }

    void FrameBufferOpenGl::Create()
    {
        // Generate and bind framebuffer
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        // Create color attachment
        glGenTextures(1, &colorbuffer);
        glBindTexture(GL_TEXTURE_2D, colorbuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuffer, 0);

        // Create depth attachment
        glGenRenderbuffers(1, &depthStencilBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer);

        CC_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is not complete!");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FrameBufferOpenGl::CreateMsaa()
    {
        glGenFramebuffers(1, &msaaFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, msaaFramebuffer);

        glGenTextures(1, &msaaColorbuffer);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaColorbuffer);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA8, width, height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, msaaColorbuffer, 0);

        glGenRenderbuffers(1, &msaaDepthStencilBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthStencilBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msaaDepthStencilBuffer);

        CC_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "MSAA framebuffer is not complete!");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FrameBufferOpenGl::Cleanup()
    {
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &colorbuffer);
        glDeleteRenderbuffers(1, &depthStencilBuffer);

        if (msaaFramebuffer != 0) {
            glDeleteFramebuffers(1, &msaaFramebuffer);
            glDeleteTextures(1, &msaaColorbuffer);
            glDeleteRenderbuffers(1, &msaaDepthStencilBuffer);
        }
    }

    void FrameBufferOpenGl::Resize(int newWidth, int newHeight)
    {
        width = newWidth;
        height = newHeight;

        glBindTexture(GL_TEXTURE_2D, colorbuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        if (samples > 0) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaColorbuffer);
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA8, width, height, GL_TRUE);

            glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthStencilBuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
        }
    }

    void FrameBufferOpenGl::Bind(int _width, int _height)
    {
        if (width != _width || height != _height)
        {
            Resize(_width, _height);
        }

        unsigned int targetFramebuffer = (samples > 0 && isMsaaEnabled) ? msaaFramebuffer : framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer);
        glViewport(0, 0, width, height);
    }

    void FrameBufferOpenGl::Resolve()
    {
        if (samples > 0 && isMsaaEnabled)
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFramebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
            glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    void FrameBufferOpenGl::BindColorBuffer()
    {
        glBindTexture(GL_TEXTURE_2D, colorbuffer);
    }

    void FrameBufferOpenGl::SetMsaaEnabled(bool isEnabled)
    {
        isMsaaEnabled = isEnabled;
    }

    bool FrameBufferOpenGl::IsMsaaEnabled() const
    {
        return isMsaaEnabled;
    }

    void FrameBufferOpenGl::SetMsaaSamples(int newSamples)
    {
        if (newSamples == samples)
        {
            return;
        }

        if (msaaFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &msaaFramebuffer);
            glDeleteTextures(1, &msaaColorbuffer);
            glDeleteRenderbuffers(1, &msaaDepthStencilBuffer);
            msaaFramebuffer = 0;
            msaaColorbuffer = 0;
            msaaDepthStencilBuffer = 0;
        }

        samples = newSamples;

        if (samples > 0)
        {
            CreateMsaa();
            isMsaaEnabled = true;
        }
    }

    int FrameBufferOpenGl::GetMsaaSamples() const
    {
        return samples;
    }
}