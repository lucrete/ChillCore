#include "PlatformWindowAndroid.h"
#include "AndroidAppContext.h"
#include "CCAssert.h"

#include <android/native_window.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <time.h>

namespace CC
{
    // ========================
    // Base singleton bookkeeping
    // ========================
    //
    // Each backend translation unit defines these once. Only one backend
    // is compiled per platform, so there is no multiple-definition risk
    // at link time.

    PlatformWindow* PlatformWindow::instance = nullptr;

    PlatformWindow::PlatformWindow()
    {
        CC_ASSERT(instance == nullptr, "PlatformWindow already created");
        instance = this;
    }

    PlatformWindow* PlatformWindow::Get()
    {
        CC_ASSERT(instance != nullptr, "PlatformWindow not created yet");
        return instance;
    }

    double PlatformWindow::GetTimeSeconds()
    {
        // CLOCK_MONOTONIC matches glfwGetTime semantics (steady, in
        // seconds) closely enough that callers using the result for
        // dt computation behave identically.
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double result = static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1.0e-9;
        return result;
    }

    // ========================
    // PlatformWindowAndroid
    // ========================

    PlatformWindowAndroid::PlatformWindowAndroid()
    {
    }

    PlatformWindowAndroid::~PlatformWindowAndroid()
    {
    }

    void PlatformWindowAndroid::Init(const Config& config)
    {
        (void)config;

        android_app* pApp = GetAndroidApp();
        CC_ASSERT(pApp != nullptr, "AndroidAppContext not initialised — call SetAndroidApp before PlatformWindow::Init");
        CC_ASSERT(pApp->window != nullptr, "PlatformWindowAndroid::Init called before APP_CMD_INIT_WINDOW");

        eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        CC_ASSERT(eglDisplay != EGL_NO_DISPLAY, "eglGetDisplay failed");

        EGLint majorVersion = 0;
        EGLint minorVersion = 0;
        EGLBoolean initSucceeded = eglInitialize(eglDisplay, &majorVersion, &minorVersion);
        CC_ASSERT(initSucceeded == EGL_TRUE, "eglInitialize failed");

        const EGLint configAttribs[] =
        {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            // Both window and pbuffer surfaces are required: the engine
            // renders to a window surface in the foreground and switches
            // to a 1×1 pbuffer while backgrounded so the EGL context
            // (and every GL handle) survives backgrounding without a
            // rebuild. See OnSurfaceLost / OnSurfaceRestored.
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_BLUE_SIZE,       8,
            EGL_GREEN_SIZE,      8,
            EGL_RED_SIZE,        8,
            EGL_ALPHA_SIZE,      8,
            EGL_DEPTH_SIZE,      24,
            EGL_STENCIL_SIZE,    8,
            EGL_NONE
        };

        EGLint numConfigs = 0;
        EGLBoolean chooseSucceeded = eglChooseConfig(eglDisplay, configAttribs, &eglConfig, 1, &numConfigs);
        CC_ASSERT(chooseSucceeded == EGL_TRUE && numConfigs > 0, "eglChooseConfig produced no matching config");

        EGLint nativeVisualId = 0;
        eglGetConfigAttrib(eglDisplay, eglConfig, EGL_NATIVE_VISUAL_ID, &nativeVisualId);
        ANativeWindow_setBuffersGeometry(pApp->window, 0, 0, nativeVisualId);

        eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, pApp->window, nullptr);
        CC_ASSERT(eglSurface != EGL_NO_SURFACE, "eglCreateWindowSurface failed");

        // GLES 3.1 minimum (matches CC_GFX_BACKEND_GLES feature requirements).
        const EGLint contextAttribs[] =
        {
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MINOR_VERSION, 1,
            EGL_NONE
        };

        eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
        CC_ASSERT(eglContext != EGL_NO_CONTEXT, "eglCreateContext failed");

        EGLBoolean makeCurrentSucceeded = eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
        CC_ASSERT(makeCurrentSucceeded == EGL_TRUE, "eglMakeCurrent failed");
    }

    void PlatformWindowAndroid::Shutdown()
    {
        if (eglDisplay != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (eglContext != EGL_NO_CONTEXT)
            {
                eglDestroyContext(eglDisplay, eglContext);
                eglContext = EGL_NO_CONTEXT;
            }
            if (eglSurface != EGL_NO_SURFACE)
            {
                eglDestroySurface(eglDisplay, eglSurface);
                eglSurface = EGL_NO_SURFACE;
            }
            if (eglPbufferSurface != EGL_NO_SURFACE)
            {
                eglDestroySurface(eglDisplay, eglPbufferSurface);
                eglPbufferSurface = EGL_NO_SURFACE;
            }

            eglTerminate(eglDisplay);
            eglDisplay = EGL_NO_DISPLAY;
        }
        eglConfig = nullptr;
    }

    // ========================
    // Surface lifecycle
    // ========================
    //
    // Preserve the EGL context across backgrounding so GL resource
    // handles survive surface loss. On surface loss the current
    // binding switches to a 1×1 pbuffer (lazily created) and the
    // on-screen surface is destroyed; on surface restored a fresh
    // window surface is created against the new ANativeWindow* and
    // rebound. The EGLContext, and every handle owned by it, is
    // unchanged across the cycle.

    void PlatformWindowAndroid::OnSurfaceLost()
    {
        if (eglDisplay != EGL_NO_DISPLAY && eglContext != EGL_NO_CONTEXT)
        {
            if (eglPbufferSurface == EGL_NO_SURFACE)
            {
                const EGLint pbufferAttribs[] =
                {
                    EGL_WIDTH,  1,
                    EGL_HEIGHT, 1,
                    EGL_NONE
                };
                eglPbufferSurface = eglCreatePbufferSurface(eglDisplay, eglConfig, pbufferAttribs);
            }

            if (eglPbufferSurface != EGL_NO_SURFACE)
            {
                eglMakeCurrent(eglDisplay, eglPbufferSurface, eglPbufferSurface, eglContext);
            }
            else
            {
                // Driver lacks pbuffer support — fall through to
                // surfaceless current. If EGL_KHR_surfaceless_context
                // is also missing, the context becomes uncurrent here;
                // that still keeps GL handles valid for restore.
                eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext);
            }

            if (eglSurface != EGL_NO_SURFACE)
            {
                eglDestroySurface(eglDisplay, eglSurface);
                eglSurface = EGL_NO_SURFACE;
            }
        }
    }

    void PlatformWindowAndroid::OnSurfaceRestored()
    {
        android_app* pApp = GetAndroidApp();
        bool canRestore = (pApp != nullptr
            && pApp->window != nullptr
            && eglDisplay != EGL_NO_DISPLAY
            && eglContext != EGL_NO_CONTEXT);

        if (canRestore)
        {
            EGLint nativeVisualId = 0;
            eglGetConfigAttrib(eglDisplay, eglConfig, EGL_NATIVE_VISUAL_ID, &nativeVisualId);
            ANativeWindow_setBuffersGeometry(pApp->window, 0, 0, nativeVisualId);

            eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, pApp->window, nullptr);
            CC_ASSERT(eglSurface != EGL_NO_SURFACE, "eglCreateWindowSurface failed during surface restore");

            EGLBoolean makeCurrentSucceeded = eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
            CC_ASSERT(makeCurrentSucceeded == EGL_TRUE, "eglMakeCurrent failed during surface restore");
        }
    }

    void PlatformWindowAndroid::GetFramebufferSize(int& width, int& height) const
    {
        // Queried fresh every call so rotation/resize is picked up
        // automatically: the EGL surface is resized by the system but
        // any cached dimensions would go stale.
        EGLint queriedWidth = 0;
        EGLint queriedHeight = 0;
        if (eglDisplay != EGL_NO_DISPLAY && eglSurface != EGL_NO_SURFACE)
        {
            eglQuerySurface(eglDisplay, eglSurface, EGL_WIDTH,  &queriedWidth);
            eglQuerySurface(eglDisplay, eglSurface, EGL_HEIGHT, &queriedHeight);
        }
        width  = queriedWidth;
        height = queriedHeight;
    }

    void PlatformWindowAndroid::SwapBuffers()
    {
        if (eglDisplay != EGL_NO_DISPLAY && eglSurface != EGL_NO_SURFACE)
        {
            EGLBoolean swapSucceeded = eglSwapBuffers(eglDisplay, eglSurface);
            if (swapSucceeded == EGL_FALSE)
            {
                EGLint error = eglGetError();
                if (error == EGL_CONTEXT_LOST)
                {
                    contextLost = true;
                }
            }
        }
    }

    void PlatformWindowAndroid::PollEvents()
    {
        // Event polling is driven by android_main's ALooper loop in
        // MainAndroid.cpp; this is a no-op so the engine's frame loop
        // does not double-pump events.
    }

    void PlatformWindowAndroid::ToggleFullscreen()
    {
        // Android always renders at the system-provided surface size.
        // Fullscreen vs windowed is not a concept at this layer.
    }

    bool PlatformWindowAndroid::IsFullscreen() const
    {
        return true;
    }

    void PlatformWindowAndroid::LockMouseCursor(bool isLocked)
    {
        (void)isLocked;
        // No mouse cursor on Android in v1.
    }

    bool PlatformWindowAndroid::IsFocused() const
    {
        // The activity only runs its frame loop while it is foregrounded,
        // so anything asking this is already focused.
        return true;
    }

    void PlatformWindowAndroid::SetFileDropCallback(std::function<void(const std::vector<std::string>&)> callback)
    {
        (void)callback;
        // Android has no analogous OS-level drag-drop gesture; the
        // override exists only to satisfy the virtual interface.
    }

    // ========================
    // Context lifecycle
    // ========================
    //
    // Context loss invalidates every EGL handle — context, surfaces,
    // and every GL object owned by them. OnContextLost tears down what
    // remains; OnContextRestored creates a fresh EGLContext and a fresh
    // window surface against the live ANativeWindow*. Engine subsystems
    // walk their CPU-authoritative sources between these two calls to
    // reissue every GL handle.

    void PlatformWindowAndroid::OnContextLost()
    {
        if (eglDisplay != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (eglSurface != EGL_NO_SURFACE)
            {
                eglDestroySurface(eglDisplay, eglSurface);
                eglSurface = EGL_NO_SURFACE;
            }
            if (eglPbufferSurface != EGL_NO_SURFACE)
            {
                eglDestroySurface(eglDisplay, eglPbufferSurface);
                eglPbufferSurface = EGL_NO_SURFACE;
            }
            if (eglContext != EGL_NO_CONTEXT)
            {
                eglDestroyContext(eglDisplay, eglContext);
                eglContext = EGL_NO_CONTEXT;
            }
        }
    }

    void PlatformWindowAndroid::OnContextRestored()
    {
        // The recovery policy on Android is to exit cleanly on context
        // loss and let the activity relaunch cold; no in-process EGL
        // recreate happens here. Hook stays as an extension point.
    }

    bool PlatformWindowAndroid::IsContextLost() const
    {
        return contextLost;
    }
}
