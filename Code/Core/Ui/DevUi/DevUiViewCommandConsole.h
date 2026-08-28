#ifndef DEVUIVIEWCOMMANDCONSOLE_H
#define DEVUIVIEWCOMMANDCONSOLE_H

#include "DevUiView.h"
#include <string>

struct ImGuiInputTextCallbackData;

namespace CC
{
    class DevUiViewCommandConsole : public DevUiView
    {
    public:
        DevUiViewCommandConsole();

        void Draw() override;
        const char* GetName() const override;
        void RequestFocus();

    private:
        static constexpr int INPUT_BUFFER_SIZE = 256;
        static constexpr int COMMAND_HISTORY_SIZE = 32;

        char inputBuffer[INPUT_BUFFER_SIZE];
        int lastLogCount = 0;
        bool shouldScrollToBottom = false;

        std::string commandHistory[COMMAND_HISTORY_SIZE];
        int historyWriteIndex = 0;
        int historyCount = 0;
        int historyBrowseIndex = -1;
        bool focusRequested = false;

        void DrawLogOutput();
        void DrawCommandInput();

        void AddToHistory(const std::string& command);

        static int InputTextCallback(ImGuiInputTextCallbackData* data);
    };
}

#endif // DEVUIVIEWCOMMANDCONSOLE_H
