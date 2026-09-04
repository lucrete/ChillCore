#ifndef PLATFORMWINDOW_H
#define PLATFORMWINDOW_H

#include <functional>
#include <string>
#include <vector>

namespace CC
{
    // ========================
    // PlatformWindow
    // ========================
    //
    // Abstract base for the OS window + GL/graphics context + related
    // platform concerns (init/terminate, fullscreen toggle, cursor mode,
    // buffer swap, time queries).

    class PlatformWindow
    {
    public:
        // Handles the graphics context was created against, for an API that
        // has to bind to an existing context rather than make its own — an
        // XR runtime being the case that needs it. Opaque on purpose: the
        // members are HDC and HGLRC on Windows and EGLDisplay and
        // EGLContext on Android, and neither type belongs in this header.
        struct NativeGraphicsBinding
        {
            void* displayOrDeviceContext = nullptr;
            void* renderContext          = nullptr;
        };

        struct Config
        {
            int         width          = 800;
            int         height         = 600;
            const char* title          = "ChillCore";
            // The desktop backend targets GL 4.3. The request was left at
            // 2.0 and relied on the driver handing back something newer,
            // which an XR runtime's graphics-requirements check does not
            // accept. Profile is left unhinted so the driver still returns a
            // compatibility context, which the developer overlay needs.
            int         glMajor        = 4;
            int         glMinor        = 3;
            bool        vsync          = true;
            bool        preferSecondaryMonitor = true;
        };

        virtual ~PlatformWindow() = default;

        static PlatformWindow* Get();

        virtual void Init(const Config& config) = 0;
        virtual void Shutdown()                 = 0;

        virtual void GetFramebufferSize(int& width, int& height) const = 0;
        virtual void SwapBuffers()                                     = 0;
        virtual void PollEvents()                                      = 0;

        virtual void ToggleFullscreen() = 0;
        virtual bool IsFullscreen() const = 0;

        virtual void LockMouseCursor(bool isLocked) = 0;

        virtual bool IsFocused() const = 0;

        // Fills outBinding with the handles the graphics context was created
        // against. Returns false where the platform cannot supply them, which
        // is the default: a platform opts in by overriding.
        virtual bool GetNativeGraphicsBinding(NativeGraphicsBinding& outBinding) const { (void)outBinding; return false; }

        // Registers a callback invoked when the OS delivers a drag-and-drop
        // payload of one or more file paths onto the window. Pass an empty
        // std::function (default-constructed) to clear. Only one consumer
        // at a time today; AppStates that care register on Init and clear
        // on Shutdown so screens that don't expect drops aren't surprised
        // by them. On Android this is a no-op since there is no analogous
        // OS gesture.
        virtual void SetFileDropCallback(std::function<void(const std::vector<std::string>&)> callback) = 0;

        // Called when the on-screen window surface goes / comes back.
        // Default no-ops; platforms whose window surface lifetime is
        // shorter than the process lifetime override these to preserve
        // the graphics context across the cycle so GL handles survive.
        virtual void OnSurfaceLost()      {}
        virtual void OnSurfaceRestored()  {}

        // Called when the underlying graphics context is destroyed and
        // recreated. Default no-ops; platforms whose context can be
        // lost (e.g. driver reset, activity destruction with the EGL
        // context attached) override to drop / recreate the context.
        virtual void OnContextLost()      {}
        virtual void OnContextRestored()  {}

        // True between detection of a context-loss and the platform
        // shell driving recovery via OnContextLost / OnContextRestored.
        // Platforms that cannot lose their context return false.
        virtual bool IsContextLost() const { return false; }

        static double GetTimeSeconds();

    protected:
        PlatformWindow();

        static PlatformWindow* instance;
    };
}

#endif // PLATFORMWINDOW_H
