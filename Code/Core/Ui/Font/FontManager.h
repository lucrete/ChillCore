#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <string>
#include <map>
#include <memory>

namespace CC
{
    class Font;

    class FontManager
    {
    public:
        FontManager();
        ~FontManager();
        static FontManager* Get();

        void LoadFont(const std::string& name, const std::string& jsonPath);
        Font* GetFont(const std::string& name) const;

    private:
        static FontManager* instance;
        std::map<std::string, std::unique_ptr<Font>> fonts;
    };
}

#endif // FONTMANAGER_H
