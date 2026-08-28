#include "StringDbManager.h"

// Suppress macro redefinition warnings from Windows headers conflicting with GLFW
#pragma warning(push)
#pragma warning(disable: 4005)
#include <rapidyaml-0.10.0.hpp>
#pragma warning(pop)

#include "CCAssert.h"
#include "PrintManager.h"
#include "PlatformFileSystem.h"

namespace CC
{
    StringDbManager* StringDbManager::instance = nullptr;
    const std::string StringDbManager::MISSING_STRING = "";

    StringDbManager::StringDbManager()
    {
        CC_ASSERT(instance == nullptr, "StringDbManager already created");
        instance = this;
    }

    StringDbManager::~StringDbManager()
    {
        instance = nullptr;
    }

    StringDbManager* StringDbManager::Get()
    {
        CC_ASSERT(instance != nullptr, "StringDbManager not created yet");
        return instance;
    }

    // ========================
    // Language management
    // ========================

    bool StringDbManager::LoadLanguage(const std::string& languageId, const std::string& filePath)
    {
        bool isSuccess = false;

        std::string content;
        if (!PlatformFileSystem::Get()->ReadFileText(filePath.c_str(), content))
        {
            CCPrint(PrintManager::CHANNEL_WARN, "StringDbManager: Failed to open: %s", filePath.c_str());
        }
        else
        {
            if (content.empty())
            {
                CCPrint(PrintManager::CHANNEL_WARN, "StringDbManager: File is empty: %s", filePath.c_str());
            }
            else
            {
                ryml::Tree tree;
                try
                {
                    tree = ryml::parse_in_arena(ryml::to_csubstr(content));
                }
                catch (const std::exception& e)
                {
                    CCPrint(PrintManager::CHANNEL_WARN, "StringDbManager: Failed to parse YAML: %s", e.what());
                }

                ryml::ConstNodeRef root = tree.rootref();

                if (root.has_child("strings"))
                {
                    auto& stringTable = languageData[languageId];
                    stringTable.clear();

                    ryml::ConstNodeRef strings = root["strings"];
                    for (ryml::ConstNodeRef child : strings.children())
                    {
                        if (child.has_key() && child.has_val())
                        {
                            c4::csubstr key = child.key();
                            c4::csubstr value = child.val();
                            std::string keyStr(key.data(), key.size());
                            std::string valueStr(value.data(), value.size());
                            stringTable[keyStr] = valueStr;
                        }
                    }

                    CCPrint(PrintManager::CHANNEL_ALWAYS, "StringDbManager: Loaded %d strings for '%s'",
                            (int)stringTable.size(), languageId.c_str());
                    isSuccess = true;
                }
                else
                {
                    CCPrint(PrintManager::CHANNEL_WARN, "StringDbManager: No 'strings' node in: %s", filePath.c_str());
                }
            }
        }

        return isSuccess;
    }

    bool StringDbManager::SetLanguage(const std::string& languageId)
    {
        bool isSuccess = false;
        auto it = languageData.find(languageId);

        if (it == languageData.end())
        {
            CCPrint(PrintManager::CHANNEL_WARN, "StringDbManager: Language not loaded: %s", languageId.c_str());
        }
        else
        {
            currentLanguage = languageId;
            activeStringTable = &it->second;
            NotifyListeners();
            isSuccess = true;
        }

        return isSuccess;
    }

    const std::string& StringDbManager::GetCurrentLanguage() const
    {
        return currentLanguage;
    }

    // ========================
    // String queries
    // ========================

    const std::string& StringDbManager::GetString(const std::string& stringId) const
    {
        const std::string* result = &MISSING_STRING;

        if (activeStringTable)
        {
            auto it = activeStringTable->find(stringId);
            if (it != activeStringTable->end())
            {
                result = &it->second;
            }
        }

        if (result == &MISSING_STRING)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "StringDbManager: Missing string: $%s", stringId.c_str());
        }

        return *result;
    }

    bool StringDbManager::HasString(const std::string& stringId) const
    {
        bool isFound = false;
        if (activeStringTable)
        {
            isFound = activeStringTable->find(stringId) != activeStringTable->end();
        }
        return isFound;
    }

    // ========================
    // Listener registration
    // ========================

    void StringDbManager::RegisterListener(IStringDbListener* listener)
    {
        listeners.push_back(listener);
    }

    void StringDbManager::UnregisterListener(IStringDbListener* listener)
    {
        bool isFound = false;
        for (auto it = listeners.begin(); it != listeners.end() && !isFound; ++it)
        {
            if (*it == listener)
            {
                listeners.erase(it);
                isFound = true;
            }
        }
    }

    void StringDbManager::Refresh()
    {
        NotifyListeners();
    }

    void StringDbManager::NotifyListeners()
    {
        for (IStringDbListener* listener : listeners)
        {
            listener->UpdateStrings();
        }
    }
}
