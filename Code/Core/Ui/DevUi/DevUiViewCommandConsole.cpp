#include "DevUiViewCommandConsole.h"
#include <string.h>
#include "imgui.h"
#include "PrintManager.h"
#include "CommandConsole.h"

namespace CC
{
    DevUiViewCommandConsole::DevUiViewCommandConsole()
        : historyWriteIndex(0)
        , historyCount(0)
        , historyBrowseIndex(-1)
        , lastLogCount(0)
        , shouldScrollToBottom(false)
    {
        memset(inputBuffer, 0, sizeof(inputBuffer));
    }

    const char* DevUiViewCommandConsole::GetName() const
    {
        return "Command Console";
    }

    // ========================
    // Draw
    // ========================

    void DevUiViewCommandConsole::Draw()
    {
        ImGui::SetNextWindowPos(ImVec2(10, 400), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(620, 300), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Command Console", &isVisible))
        {
            DrawLogOutput();
            DrawCommandInput();
        }
        ImGui::End();
    }

    // ========================
    // Log Output
    // ========================

    void DevUiViewCommandConsole::DrawLogOutput()
    {
        float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("LogRegion", ImVec2(0, -footerHeight), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

        PrintManager* printManager = PrintManager::Get();
        const PrintManager::LogEntry* logBuffer = printManager->GetLogBuffer();
        int logCount = printManager->GetLogCount();
        int writeIndex = printManager->GetLogWriteIndex();

        if (logCount != lastLogCount)
        {
            shouldScrollToBottom = true;
            lastLogCount = logCount;
        }

        int startIndex = 0;
        if (logCount >= PrintManager::LOG_BUFFER_SIZE)
        {
            startIndex = writeIndex;
        }

        for (int i = 0; i < logCount; i++)
        {
            int index = (startIndex + i) % PrintManager::LOG_BUFFER_SIZE;
            const PrintManager::LogEntry& entry = logBuffer[index];

            ImVec4 color;
            switch (entry.channel)
            {
            case PrintManager::CHANNEL_WARN:
                color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                break;
            case PrintManager::CHANNEL_SHADER:
                color = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
                break;
            case PrintManager::CHANNEL_ALWAYS:
                color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                break;
            default:
                color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                break;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(entry.message.c_str());
            ImGui::PopStyleColor();
        }

        if (shouldScrollToBottom)
        {
            ImGui::SetScrollHereY(1.0f);
            shouldScrollToBottom = false;
        }

        ImGui::EndChild();
    }

    // ========================
    // Command Input
    // ========================

    void DevUiViewCommandConsole::DrawCommandInput()
    {
        ImGui::Separator();

        ImGui::Text(">");
        ImGui::SameLine();

        ImGuiInputTextFlags inputFlags =
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CallbackHistory;

        bool reclaimFocus = false;

        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##CommandInput", inputBuffer, INPUT_BUFFER_SIZE, inputFlags, InputTextCallback, this))
        {
            std::string command(inputBuffer);
            if (!command.empty())
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "> %s", command.c_str());
                AddToHistory(command);
                CommandConsole::Get()->Execute(command);
            }
            memset(inputBuffer, 0, sizeof(inputBuffer));
            reclaimFocus = true;
            historyBrowseIndex = -1;
        }
        ImGui::PopItemWidth();

        ImGui::SetItemDefaultFocus();
        if (reclaimFocus)
        {
            ImGui::SetKeyboardFocusHere(-1);
        }
        if (focusRequested)
        {
            ImGui::SetKeyboardFocusHere(-1);
            focusRequested = false;
        }
    }

    void DevUiViewCommandConsole::RequestFocus()
    {
        focusRequested = true;
    }

    // ========================
    // Command History
    // ========================

    void DevUiViewCommandConsole::AddToHistory(const std::string& command)
    {
        commandHistory[historyWriteIndex] = command;
        historyWriteIndex = (historyWriteIndex + 1) % COMMAND_HISTORY_SIZE;
        if (historyCount < COMMAND_HISTORY_SIZE)
        {
            historyCount++;
        }
    }

    int DevUiViewCommandConsole::InputTextCallback(ImGuiInputTextCallbackData* data)
    {
        DevUiViewCommandConsole* console = static_cast<DevUiViewCommandConsole*>(data->UserData);

        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
        {
            if (console->historyCount == 0)
            {
                return 0;
            }

            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (console->historyBrowseIndex == -1)
                {
                    console->historyBrowseIndex = console->historyCount - 1;
                }
                else if (console->historyBrowseIndex > 0)
                {
                    console->historyBrowseIndex--;
                }
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (console->historyBrowseIndex != -1)
                {
                    console->historyBrowseIndex++;
                    if (console->historyBrowseIndex >= console->historyCount)
                    {
                        console->historyBrowseIndex = -1;
                    }
                }
            }

            if (console->historyBrowseIndex >= 0)
            {
                int oldest = 0;
                if (console->historyCount >= COMMAND_HISTORY_SIZE)
                {
                    oldest = console->historyWriteIndex;
                }
                int index = (oldest + console->historyBrowseIndex) % COMMAND_HISTORY_SIZE;
                const std::string& historyEntry = console->commandHistory[index];

                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, historyEntry.c_str());
            }
            else
            {
                data->DeleteChars(0, data->BufTextLen);
            }
        }

        return 0;
    }
}
