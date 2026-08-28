#ifndef STRINGDBMANAGER_H
#define STRINGDBMANAGER_H

#include <string>
#include <unordered_map>
#include <vector>

namespace CC
{
    class IStringDbListener
    {
    public:
        virtual ~IStringDbListener() = default;
        virtual void UpdateStrings() = 0;
    };

    class StringDbManager
    {
    public:
        StringDbManager();
        ~StringDbManager();
        static StringDbManager* Get();

        // ========================
        // Language management
        // ========================

        bool LoadLanguage(const std::string& languageId, const std::string& filePath);
        bool SetLanguage(const std::string& languageId);
        const std::string& GetCurrentLanguage() const;

        // ========================
        // String queries
        // ========================

        const std::string& GetString(const std::string& stringId) const;
        bool HasString(const std::string& stringId) const;

        // ========================
        // Listener registration
        // ========================

        void RegisterListener(IStringDbListener* listener);
        void UnregisterListener(IStringDbListener* listener);
        void Refresh();

    private:
        static StringDbManager* instance;

        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> languageData;
        const std::unordered_map<std::string, std::string>* activeStringTable = nullptr;
        std::string currentLanguage;

        static const std::string MISSING_STRING;

        std::vector<IStringDbListener*> listeners;

        void NotifyListeners();
    };
}

#endif // STRINGDBMANAGER_H
