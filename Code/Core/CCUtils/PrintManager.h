#ifndef PRINTMANAGER_H
#define PRINTMANAGER_H
#include <string>

#ifdef CC_DEBUG
#ifndef CC_PRINT_ENABLED
#define CC_PRINT_ENABLED
#endif
#endif

namespace CC
{
    class PrintManager
    {
    public:
        enum Channel
        {
            CHANNEL_ALWAYS,
            CHANNEL_WARN,
            CHANNEL_FRAMEWORK,
            CHANNEL_FRAMETIME,
            CHANNEL_RENDER,
            CHANNEL_INPUT,
            CHANNEL_UNITTEST,
            CHANNEL_SHADER,
            CHANNEL_MAX
        };

        struct LogEntry
        {
            std::string message;
            Channel channel = CHANNEL_ALWAYS;
        };

        PrintManager();
        virtual ~PrintManager();

        static PrintManager* Get();
        void Print(Channel channel, const char* format, ...);

        const LogEntry* GetLogBuffer() const;
        int GetLogCount() const;
        int GetLogWriteIndex() const;
        void ClearLog();

        bool IsChannelEnabled(Channel channel) const;
        void SetChannelEnabled(Channel channel, bool isEnabled);
        const char* GetChannelName(Channel channel) const;

        static constexpr int LOG_BUFFER_SIZE = 512;

    private:
        static PrintManager* instance;
        bool channels[CHANNEL_MAX];
        LogEntry logBuffer[LOG_BUFFER_SIZE];
        int logWriteIndex = 0;
        int logCount = 0;
    };
}

#ifdef CC_PRINT_ENABLED
    #define CCPrint CC::PrintManager::Get()->Print
#else
    #define CCPrint(...)
#endif
#endif // PRINTMANAGER_H
