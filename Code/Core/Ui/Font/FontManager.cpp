#include "FontManager.h"
#include "Font.h"
#include "CCAssert.h"
#include "PrintManager.h"

namespace CC
{
    FontManager* FontManager::instance = nullptr;

    FontManager::FontManager()
    {
        CC_ASSERT(instance == nullptr, "FontManager already created");
        instance = this;

        LoadFont("robotoRegular", "Data\\Fonts\\robotoRegular.json");
    }

    FontManager::~FontManager()
    {
        fonts.clear();
        instance = nullptr;
    }

    FontManager* FontManager::Get()
    {
        CC_ASSERT(instance != nullptr, "FontManager not created yet");
        return instance;
    }

    void FontManager::LoadFont(const std::string& name, const std::string& jsonPath)
    {
        auto it = fonts.find(name);
        if (it != fonts.end())
        {
            CCPrint(PrintManager::CHANNEL_WARN, "FontManager: Font already loaded: %s", name.c_str());
            return;
        }

        fonts[name] = std::make_unique<Font>(jsonPath);
    }

    Font* FontManager::GetFont(const std::string& name) const
    {
        auto it = fonts.find(name);
        if (it != fonts.end())
        {
            return it->second.get();
        }
        CCPrint(PrintManager::CHANNEL_WARN, "FontManager: Font not found: %s", name.c_str());
        return nullptr;
    }
}
