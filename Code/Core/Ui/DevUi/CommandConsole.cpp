#include "CommandConsole.h"
#include <sstream>
#include "CCAssert.h"
#include "PrintManager.h"
#include "RenderManager.h"

namespace CC
{
    CommandConsole* CommandConsole::instance = nullptr;

    CommandConsole::CommandConsole()
    {
        CC_ASSERT(instance == nullptr, "CommandConsole already created");
        instance = this;
        RegisterBuiltInCommands();
    }

    CommandConsole::~CommandConsole()
    {
        instance = nullptr;
    }

    CommandConsole* CommandConsole::Get()
    {
        CC_ASSERT(instance != nullptr, "CommandConsole not created yet");
        return instance;
    }

    void CommandConsole::RegisterCommand(const std::string& name, const std::string& description, CommandCallback callback)
    {
        commands[name] = { description, callback };
    }

    void CommandConsole::Execute(const std::string& commandLine)
    {
        std::vector<std::string> tokens = ParseCommandLine(commandLine);
        if (tokens.empty())
        {
            return;
        }

        std::string commandName = tokens[0];
        std::vector<std::string> args(tokens.begin() + 1, tokens.end());

        auto it = commands.find(commandName);
        if (it != commands.end())
        {
            it->second.callback(args);
        }
        else
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "Unknown command: %s", commandName.c_str());
        }
    }

    const std::map<std::string, CommandDef>& CommandConsole::GetCommands() const
    {
        return commands;
    }

    // ========================
    // Built-in Commands
    // ========================

    void CommandConsole::RegisterBuiltInCommands()
    {
        RegisterCommand("help", "List all available commands", [this](const std::vector<std::string>&)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "Available commands:");
            for (const auto& pair : commands)
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "  %s - %s", pair.first.c_str(), pair.second.description.c_str());
            }
        });

        RegisterCommand("clear", "Clear the log", [](const std::vector<std::string>&)
        {
            PrintManager::Get()->ClearLog();
        });

        RegisterCommand("channels", "List or toggle print channels (usage: channels [name] [on|off])", [](const std::vector<std::string>& args)
        {
            PrintManager* printManager = PrintManager::Get();

            if (args.empty())
            {
                for (int i = 0; i < PrintManager::CHANNEL_MAX; i++)
                {
                    PrintManager::Channel channel = static_cast<PrintManager::Channel>(i);
                    const char* state = printManager->IsChannelEnabled(channel) ? "on" : "off";
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "  %s: %s", printManager->GetChannelName(channel), state);
                }
                return;
            }

            if (args.size() >= 2)
            {
                const std::string& channelName = args[0];
                const std::string& stateStr = args[1];

                for (int i = 0; i < PrintManager::CHANNEL_MAX; i++)
                {
                    PrintManager::Channel channel = static_cast<PrintManager::Channel>(i);
                    if (channelName == printManager->GetChannelName(channel))
                    {
                        bool isEnabled = (stateStr == "on");
                        printManager->SetChannelEnabled(channel, isEnabled);
                        CCPrint(PrintManager::CHANNEL_ALWAYS, "Channel %s set to %s", channelName.c_str(), stateStr.c_str());
                        return;
                    }
                }
                CCPrint(PrintManager::CHANNEL_ALWAYS, "Unknown channel: %s", channelName.c_str());
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "Usage: channels [name] [on|off]");
            }
        });

        RegisterCommand("echo", "Print a message", [](const std::vector<std::string>& args)
        {
            std::string message;
            for (size_t i = 0; i < args.size(); i++)
            {
                if (i > 0)
                {
                    message += " ";
                }
                message += args[i];
            }
            CCPrint(PrintManager::CHANNEL_ALWAYS, "%s", message.c_str());
        });

        RegisterCommand("render", "Render settings (usage: render aa [on|off|2|4|8])", [](const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "Usage: render aa [on|off|2|4|8]");
                return;
            }

            if (args[0] == "aa")
            {
                if (args.size() < 2)
                {
                    const char* state = RenderManager::Get()->IsAntialiasingEnabled() ? "on" : "off";
                    int samples = RenderManager::Get()->GetMsaaSamples();
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "Antialiasing: %s (%dx)", state, samples);
                    return;
                }

                if (args[1] == "on")
                {
                    RenderManager::Get()->SetAntialiasingEnabled(true);
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "Antialiasing enabled");
                }
                else if (args[1] == "off")
                {
                    RenderManager::Get()->SetAntialiasingEnabled(false);
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "Antialiasing disabled");
                }
                else if (args[1] == "2" || args[1] == "4" || args[1] == "8")
                {
                    int samples = std::stoi(args[1]);
                    RenderManager::Get()->SetMsaaSamples(samples);
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "Antialiasing set to %dx", samples);
                }
                else
                {
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "Usage: render aa [on|off|2|4|8]");
                }
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "Unknown render setting: %s", args[0].c_str());
            }
        });
    }

    // ========================
    // Parsing
    // ========================

    std::vector<std::string> CommandConsole::ParseCommandLine(const std::string& commandLine) const
    {
        std::vector<std::string> tokens;
        std::istringstream stream(commandLine);
        std::string token;
        while (stream >> token)
        {
            tokens.push_back(token);
        }
        return tokens;
    }
}
