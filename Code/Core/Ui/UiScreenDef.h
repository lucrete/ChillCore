#ifndef UISCREENDEF_H
#define UISCREENDEF_H

#include <string>
#include <vector>
#include "UiCallbackMap.h"
#include "UiCssParser.h"

namespace CC
{
    class UiScreenController;
    class UiElement;

    struct UiScreenDef
    {
        std::string htmlPath;
        std::string cssPath;
        UiScreenController* controller = nullptr;
        UiElement* rootElement = nullptr;
        UiCallbackMap callbackMap;

        // Retained so dynamically-created elements (built by controllers
        // at runtime, e.g. lists populated from data) can be styled with
        // the same rules the static HTML used. UiHtmlParser::Parse
        // applies rules at parse time and discards them, so without this
        // cache later mutations would render unstyled.
        std::vector<UiCssRule> cssRules;
    };
}

#endif // UISCREENDEF_H
