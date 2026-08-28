#ifndef PLATFORMWINDOWANDROID_H
#define PLATFORMWINDOWANDROID_H

#include "PlatformWindow.h"
#include <EGL/egl.h>

namespace CC
{
    // ========================
    // PlatformWindowAndroid
    // ========================
    //
    // EGL-backed PlatformWindow for Android. The ANativeWindow is owned
    // by GameActivity; this class only attaches an EGL surface + context
    // to it. The android_app pointer is retrieved via the global
    // AndroidAppContext, which MainAndroid.cpp populates before calling
    // CoreMain::Init.
    //
    // Lifecycle expectation: Init() may only be called once
    // APP_CMD_INIT_WINDOW has fired and android_app->window is non-null.
    // ToggleFullscreen and LockMouseCursor are no-ops — Android has no
    // analogous concepts at this layer.

    class PlatformWindowAndroid : public PlatformWindow
    {
    public:
        PlatformWindowAndroid();
        virtual ~PlatformWindowAndroid();

        virtual void Init(const Config& config) override;
        virtual void Shutdown()                 override;

        virtual void GetFramebufferSize(int& width, int& height) const override;
        virtual void SwapBuffers()                                     override;
        virtual void PollEvents()                                      override;

        virtual void ToggleFullscreen() override;
        virtual bool IsFullscreen() const override;

        virtual void LockMouseCursor(bool isLocked) override;

        virtual void SetFileDropCallback(std::function<void(const std::vector<std::string>&)> callback) override;

        virtual void OnSurfaceLost()      override;
        virtual void OnSurfaceRestored()  override;

        virtual void OnContextLost()      override;
        virtual void OnContextRestored()  override;
        virtual bool IsContextLost() const override;

    private:
        EGLDisplay eglDisplay        = EGL_NO_DISPLAY;
        EGLContext eglContext        = EGL_NO_CONTEXT;
        EGLSurface eglSurface        = EGL_NO_SURFACE;
        EGLSurface eglPbufferSurface = EGL_NO_SURFACE;
        EGLConfig  eglConfig         = nullptr;
        bool       contextLost       = false;
    };
}

#endif // PLATFORMWINDOWANDROID_H
