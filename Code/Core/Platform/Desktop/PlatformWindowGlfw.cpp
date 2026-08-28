#include "PlatformWindowGlfw.h"
#include "CCAssert.h"
#include "CoreMain.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

namespace CC
{
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
        return glfwGetTime();
    }

    static void ErrorCallback(int error, const char* description)
    {
        fprintf(stderr, "Error: %s\n", description);
    }

    static void WindowCloseCallback(GLFWwindow* window)
    {
        CoreMain::Get()->RequestQuit();
    }

    PlatformWindowGlfw::PlatformWindowGlfw()
        : window(nullptr)
        , isFullscreen(false)
        , windowedMode{ 0, 0, 0, 0 }
    {
    }

    PlatformWindowGlfw::~PlatformWindowGlfw()
    {
        instance = nullptr;
    }

    void PlatformWindowGlfw::Init(const Config& config)
    {
        glfwSetErrorCallback(ErrorCallback);

        if (!glfwInit())
        {
            exit(EXIT_FAILURE);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, config.glMajor);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, config.glMinor);

        window = glfwCreateWindow(config.width, config.height, config.title, NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            exit(EXIT_FAILURE);
        }

        // Place on the secondary monitor when available — handy on a
        // dev rig so the app doesn't land behind the IDE. Falls back to
        // the primary monitor otherwise.
        if (config.preferSecondaryMonitor)
        {
            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            GLFWmonitor* monitor = (monitorCount > 1) ? monitors[1] : glfwGetPrimaryMonitor();
            int xpos = 0;
            int ypos = 0;
            glfwGetMonitorPos(monitor, &xpos, &ypos);
            const int xOffset = 20;
            const int yOffset = 100;
            glfwSetWindowPos(window, xpos + xOffset, ypos + yOffset);
        }

        glfwSetWindowCloseCallback(window, WindowCloseCallback);

        // glfw routes drop events to a window-bound callback. Stash this
        // instance in the window user pointer so the static dispatcher
        // can recover it and forward to the std::function we hold.
        glfwSetWindowUserPointer(window, this);
        glfwSetDropCallback(window, GlfwDropCallback);

        glfwMakeContextCurrent(window);
        glfwSwapInterval(config.vsync ? 1 : 0);
    }

    void PlatformWindowGlfw::GlfwDropCallback(GLFWwindow* window, int pathCount, const char* paths[])
    {
        PlatformWindowGlfw* self = (PlatformWindowGlfw*)glfwGetWindowUserPointer(window);
        if (self != nullptr && self->fileDropCallback)
        {
            std::vector<std::string> pathList;
            pathList.reserve((size_t)pathCount);
            for (int i = 0; i < pathCount; i++)
            {
                pathList.push_back(paths[i]);
            }
            self->fileDropCallback(pathList);
        }
    }

    void PlatformWindowGlfw::SetFileDropCallback(std::function<void(const std::vector<std::string>&)> callback)
    {
        fileDropCallback = std::move(callback);
    }

    void PlatformWindowGlfw::Shutdown()
    {
        if (window != nullptr)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
    }

    void PlatformWindowGlfw::GetFramebufferSize(int& width, int& height) const
    {
        glfwGetFramebufferSize(window, &width, &height);
    }

    void PlatformWindowGlfw::SwapBuffers()
    {
        glfwSwapBuffers(window);
    }

    void PlatformWindowGlfw::PollEvents()
    {
        glfwPollEvents();
    }

    bool PlatformWindowGlfw::IsFullscreen() const
    {
        return isFullscreen;
    }

    void PlatformWindowGlfw::ToggleFullscreen()
    {
        if (isFullscreen)
        {
            glfwSetWindowMonitor(window, nullptr,
                windowedMode.xpos, windowedMode.ypos,
                windowedMode.width, windowedMode.height, 0);
        }
        else
        {
            glfwGetWindowPos(window, &windowedMode.xpos, &windowedMode.ypos);
            glfwGetWindowSize(window, &windowedMode.width, &windowedMode.height);

            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }

        isFullscreen = !isFullscreen;
    }

    void PlatformWindowGlfw::LockMouseCursor(bool isLocked)
    {
        glfwSetInputMode(window, GLFW_CURSOR, isLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}
