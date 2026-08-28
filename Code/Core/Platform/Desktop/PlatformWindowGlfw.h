#ifndef PLATFORMWINDOWGLFW_H
#define PLATFORMWINDOWGLFW_H

#include "PlatformWindow.h"

struct GLFWwindow;

namespace CC
{
    class PlatformWindowGlfw : public PlatformWindow
    {
    public:
        PlatformWindowGlfw();
        virtual ~PlatformWindowGlfw();

        virtual void Init(const Config& config) override;
        virtual void Shutdown()                 override;

        virtual void GetFramebufferSize(int& width, int& height) const override;
        virtual void SwapBuffers()                                     override;
        virtual void PollEvents()                                      override;

        virtual void ToggleFullscreen() override;
        virtual bool IsFullscreen() const override;

        virtual void LockMouseCursor(bool isLocked) override;

        virtual void SetFileDropCallback(std::function<void(const std::vector<std::string>&)> callback) override;

    private:
        static void GlfwDropCallback(GLFWwindow* window, int pathCount, const char* paths[]);

        GLFWwindow*                                                    window;
        bool                                                           isFullscreen;
        std::function<void(const std::vector<std::string>&)>           fileDropCallback;

        struct WindowProperties
        {
            int xpos;
            int ypos;
            int width;
            int height;
        } windowedMode;
    };
}

#endif // PLATFORMWINDOWGLFW_H
