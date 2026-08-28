#ifndef FRAMEBUFFEROPENGL_H
#define FRAMEBUFFEROPENGL_H

namespace CC
{
    class FrameBufferOpenGl {
    public:
        FrameBufferOpenGl(int initialWidth, int initialHeight, int samples = 0);
        ~FrameBufferOpenGl();

        void Create();
        void CreateMsaa();
        void Cleanup();
        void Resize(int newWidth, int newHeight);
        void Bind(int width, int height);
        void BindColorBuffer();
        void Resolve();

        void SetMsaaEnabled(bool isEnabled);
        bool IsMsaaEnabled() const;

        void SetMsaaSamples(int newSamples);
        int GetMsaaSamples() const;

    private:
        FrameBufferOpenGl();

        int width;
        int height;
        int samples;

        unsigned int framebuffer;
        unsigned int colorbuffer;
        unsigned int depthStencilBuffer;

        bool isMsaaEnabled = true;

        unsigned int msaaFramebuffer = 0;
        unsigned int msaaColorbuffer = 0;
        unsigned int msaaDepthStencilBuffer = 0;
    };
}

#endif // FRAMEBUFFEROPENGL_H