#ifndef COMMANDCONSOLE_H
#define COMMANDCONSOLE_H

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace CC
{
    using CommandCallback = std::function<void(const std::vector<std::string>& args)>;

    struct CommandDef
    {
        std::string description;
        CommandCallback callback;
    };

    class CommandConsole
    {
    public:
        CommandConsole();
        ~CommandConsole();

        static CommandConsole* Get();

        void RegisterCommand(const std::string& name, const std::string& description, CommandCallback callback);
        void Execute(const std::string& commandLine);
        const std::map<std::string, CommandDef>& GetCommands() const;

    private:
        static CommandConsole* instance;

        std::map<std::string, CommandDef> commands;

        void RegisterBuiltInCommands();
        std::vector<std::string> ParseCommandLine(const std::string& commandLine) const;
    };
}

#endif // COMMANDCONSOLE_H
