#ifndef UIHTMLPARSER_H
#define UIHTMLPARSER_H

#include <string>
#include <vector>
#include "UiCssParser.h"

namespace CC
{
    class UiElement;

    class UiHtmlParser
    {
    public:
        static UiElement* Parse(const std::string& htmlPath, const std::vector<UiCssRule>& cssRules);

        // Re-applies cssRules to an element subtree and resolves
        // inheritance from the parent's already-computed style. Used
        // when a controller adds elements at runtime (e.g. list rows
        // built from data) — Parse applies styles only at HTML-parse
        // time, so dynamic subtrees would otherwise render unstyled.
        static void ApplyStylesToSubtree(UiElement* element, const std::vector<UiCssRule>& cssRules);

    private:
        static UiElement* ParseNode(void* xmlNode);
        static void ApplyCssRules(UiElement* root, const std::vector<UiCssRule>& cssRules);
        static void ApplyRulesToElement(UiElement* element, const std::vector<UiCssRule>& cssRules);
        static void ResolveInheritance(UiElement* element);
    };
}

#endif // UIHTMLPARSER_H
