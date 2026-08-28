#ifndef UICSSPARSER_H
#define UICSSPARSER_H

#include <string>
#include <vector>
#include "UiStyle.h"

namespace CC
{
    struct UiCssRule
    {
        std::string selector;
        std::string className;
        std::string pseudoClass;
        UiStyleProperties properties;
    };

    class UiCssParser
    {
    public:
        static std::vector<UiCssRule> Parse(const std::string& cssText);

    private:
        static void ApplyProperty(UiStyleProperties& properties, const std::string& name, const std::string& value);
        static void ParseEdgeShorthand(const std::string& value, UiEdgeInsets& edges);
        static std::string Trim(const std::string& str);
    };
}

#endif // UICSSPARSER_H
