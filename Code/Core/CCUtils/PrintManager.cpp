#include "PrintManager.h"
#include <stdio.h>
#include <stdarg.h>
#include "CCAssert.h"
#include <memory.h>

#ifdef _WIN32
    #include <Windows.h>
#endif

#ifdef __ANDROID__
    #include <android/log.h>
#endif


namespace CC
{
    PrintManager* PrintManager::instance = NULL;

    PrintManager::PrintManager()
    {
        CC_ASSERT(NULL == instance, "PrintManager already created");
        instance = this;

        memset(channels, 0, sizeof(channels[0] * CHANNEL_MAX));

        // Set these to true to enable a channel
        channels[CHANNEL_ALWAYS] = true;
        channels[CHANNEL_WARN] = true;
        channels[CHANNEL_FRAMEWORK] = false;
        channels[CHANNEL_FRAMETIME] = true;
        channels[CHANNEL_RENDER] = true;
        channels[CHANNEL_INPUT] = false;
        channels[CHANNEL_UNITTEST] = true;
        channels[CHANNEL_SHADER] = true;

#ifdef _WIN32
        // Windows-specific code to move the console window
        // Absolute values for a specific monitor setup
        HWND consoleWindow = GetConsoleWindow();
        if (NULL != consoleWindow)
        {
            const int xOffset = 80;
            const int yOffset = 80;
            SetWindowPos(consoleWindow, 0, xOffset, yOffset, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
#endif
    }

    PrintManager::~PrintManager()
    {
#ifdef _WIN32
        // Windows-specific code to close the console window
        // when running in Visual Studio
        HWND consoleWindow = GetConsoleWindow();
        if (NULL != consoleWindow)
        {
            PostMessage(consoleWindow, WM_CLOSE, 0, 0);
        }
#endif
        instance = NULL;
    }

    PrintManager* PrintManager::PrintManager::Get()
    {
        CC_ASSERT(NULL != instance, "PrintManager not created yet");
        return instance;
    }

    void PrintManager::Print(Channel channel, const char* format, ...)
    {
        if (channels[channel])
        {
            char buffer[1024];
            va_list args;
            va_start(args, format);
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);

#ifdef __ANDROID__
            // logcat tag matches MainAndroid.cpp's tag for unified filtering.
            __android_log_print(ANDROID_LOG_INFO, "ChillCore", "%s", buffer);
#else
            printf("%s\n", buffer);
#endif

            logBuffer[logWriteIndex].message = buffer;
            logBuffer[logWriteIndex].channel = channel;
            logWriteIndex = (logWriteIndex + 1) % LOG_BUFFER_SIZE;
            if (logCount < LOG_BUFFER_SIZE)
            {
                logCount++;
            }
        }
    }

    const PrintManager::LogEntry* PrintManager::GetLogBuffer() const
    {
        return logBuffer;
    }

    int PrintManager::GetLogCount() const
    {
        return logCount;
    }

    int PrintManager::GetLogWriteIndex() const
    {
        return logWriteIndex;
    }

    void PrintManager::ClearLog()
    {
        logWriteIndex = 0;
        logCount = 0;
    }

    bool PrintManager::IsChannelEnabled(Channel channel) const
    {
        return channels[channel];
    }

    void PrintManager::SetChannelEnabled(Channel channel, bool isEnabled)
    {
        channels[channel] = isEnabled;
    }

    const char* PrintManager::GetChannelName(Channel channel) const
    {
        static const char* CHANNEL_NAMES[CHANNEL_MAX] =
        {
            "Always",
            "Warn",
            "Framework",
            "FrameTime",
            "Render",
            "Input",
            "UnitTest",
            "Shader"
        };
        if (channel >= 0 && channel < CHANNEL_MAX)
        {
            return CHANNEL_NAMES[channel];
        }
        return "Unknown";
    }
}