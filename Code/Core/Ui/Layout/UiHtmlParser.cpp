#include "UiHtmlParser.h"

#include <sstream>
#include "pugixml.hpp"
#include "UiElement.h"
#include "UiPanel.h"
#include "UiButton.h"
#include "UiText.h"
#include "UiSlider.h"
#include "UiToggle.h"
#include "UiDropdown.h"
#include "UiJoystick.h"
#include "PrintManager.h"
#include "PlatformFileSystem.h"

namespace CC
{
    UiElement* UiHtmlParser::Parse(const std::string& htmlPath, const std::vector<UiCssRule>& cssRules)
    {
        UiElement* root = nullptr;

        std::string htmlContents;
        if (!PlatformFileSystem::Get()->ReadFileText(htmlPath.c_str(), htmlContents))
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to open HTML file: %s", htmlPath.c_str());
        }
        else
        {
            pugi::xml_document doc;
            pugi::xml_parse_result result = doc.load_buffer(htmlContents.data(), htmlContents.size());
            if (!result)
            {
                CCPrint(PrintManager::CHANNEL_WARN, "Failed to parse HTML file: %s - %s", htmlPath.c_str(), result.description());
            }
            else
            {
                pugi::xml_node rootNode = doc.first_child();
                if (!rootNode)
                {
                    CCPrint(PrintManager::CHANNEL_WARN, "HTML file has no root element: %s", htmlPath.c_str());
                }
                else
                {
                    // If root is <html>, look for <body>
                    if (std::string(rootNode.name()) == "html")
                    {
                        pugi::xml_node bodyNode = rootNode.child("body");
                        if (bodyNode)
                        {
                            rootNode = bodyNode;
                        }
                    }

                    root = ParseNode(&rootNode);
                    if (root)
                    {
                        ApplyCssRules(root, cssRules);
                        ResolveInheritance(root);
                        root->RecomputeActiveStyle();
                    }
                }
            }
        }

        return root;
    }

    UiElement* UiHtmlParser::ParseNode(void* xmlNodePtr)
    {
        pugi::xml_node& node = *static_cast<pugi::xml_node*>(xmlNodePtr);

        std::string tagName = node.name();

        // Determine element type from tag
        UiElementType elementType = UiElementType::Panel;
        if (tagName == "button")
        {
            elementType = UiElementType::Button;
        }
        else if (tagName == "span" || tagName == "p" || tagName == "label" || tagName == "h1" ||
            tagName == "h2" || tagName == "h3")
        {
            elementType = UiElementType::Text;
        }
        else if (tagName == "input")
        {
            std::string inputType = node.attribute("type").as_string();
            if (inputType == "range")
            {
                elementType = UiElementType::Slider;
            }
            else if (inputType == "checkbox")
            {
                elementType = UiElementType::Toggle;
            }
        }
        else if (tagName == "select")
        {
            elementType = UiElementType::Dropdown;
        }
        else if (tagName == "joystick")
        {
            elementType = UiElementType::Joystick;
        }

        // Create specialized element
        UiElement* element = nullptr;
        switch (elementType)
        {
        case UiElementType::Panel:
            element = new UiPanel();
            break;
        case UiElementType::Button:
            element = new UiButton();
            break;
        case UiElementType::Text:
        {
            bool isBlockLevel = (tagName == "p" || tagName == "h1" || tagName == "h2" || tagName == "h3");
            element = new UiText(isBlockLevel);
            break;
        }
        case UiElementType::Slider:
        {
            UiSlider* slider = new UiSlider();
            slider->SetMinValue(node.attribute("min").as_float(0.0f));
            slider->SetMaxValue(node.attribute("max").as_float(100.0f));
            slider->SetCurrentValue(node.attribute("value").as_float(50.0f));
            element = slider;
            break;
        }
        case UiElementType::Toggle:
        {
            UiToggle* toggle = new UiToggle();
            // HTML boolean attribute: presence means true
            toggle->SetChecked(!node.attribute("checked").empty());
            element = toggle;
            break;
        }
        case UiElementType::Joystick:
        {
            element = new UiJoystick();
            break;
        }
        case UiElementType::Dropdown:
        {
            UiDropdown* dropdown = new UiDropdown();
            int selectedIdx = 0;
            int optIdx = 0;
            for (pugi::xml_node optNode = node.child("option"); optNode; optNode = optNode.next_sibling("option"))
            {
                std::string optText = optNode.child_value();
                // Trim whitespace
                size_t s = optText.find_first_not_of(" \t\r\n");
                size_t e = optText.find_last_not_of(" \t\r\n");
                if (s != std::string::npos)
                {
                    optText = optText.substr(s, e - s + 1);
                }
                dropdown->AddOption(optText);
                if (optNode.attribute("selected"))
                {
                    selectedIdx = optIdx;
                }
                optIdx++;
            }
            dropdown->SetSelectedOption(selectedIdx);
            dropdown->SetTextContent(dropdown->GetSelectedText());
            element = dropdown;
            break;
        }
        default:
            element = new UiElement(elementType);
            break;
        }

        // Read id
        pugi::xml_attribute idAttr = node.attribute("id");
        if (idAttr)
        {
            element->SetId(idAttr.as_string());
        }

        // Read classes
        pugi::xml_attribute classAttr = node.attribute("class");
        if (classAttr)
        {
            std::istringstream classStream(classAttr.as_string());
            std::string className;
            while (classStream >> className)
            {
                element->AddClass(className);
            }
        }

        // Read data-action
        pugi::xml_attribute actionAttr = node.attribute("data-action");
        if (actionAttr)
        {
            element->SetDataAction(actionAttr.as_string());
        }

        // Set navigable for buttons
        if (elementType == UiElementType::Button || elementType == UiElementType::Slider ||
            elementType == UiElementType::Toggle || elementType == UiElementType::Dropdown ||
            elementType == UiElementType::Joystick)
        {
            element->SetNavigable(true);
        }

        // Read text content (direct text children only)
        std::string textContent;
        for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
        {
            if (child.type() == pugi::node_pcdata)
            {
                std::string text = child.value();
                // Trim whitespace
                size_t start = text.find_first_not_of(" \t\r\n");
                size_t end = text.find_last_not_of(" \t\r\n");
                if (start != std::string::npos)
                {
                    if (!textContent.empty())
                    {
                        textContent += " ";
                    }
                    textContent += text.substr(start, end - start + 1);
                }
            }
        }
        if (!textContent.empty())
        {
            element->SetTextContent(textContent);
        }

        // Recurse into child elements
        // - Dropdown: <option> children are parsed separately
        // - Joystick: knob child is auto-created in UiJoystick ctor
        if (elementType != UiElementType::Dropdown && elementType != UiElementType::Joystick)
        {
            for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
            {
                if (child.type() == pugi::node_element)
                {
                    UiElement* childElement = ParseNode(&child);
                    if (childElement)
                    {
                        element->AddChild(childElement);
                    }
                }
            }
        }

        return element;
    }

    void UiHtmlParser::ApplyCssRules(UiElement* root, const std::vector<UiCssRule>& cssRules)
    {
        ApplyRulesToElement(root, cssRules);
        for (UiElement* child : root->GetChildren())
        {
            ApplyCssRules(child, cssRules);
        }
    }

    void UiHtmlParser::ApplyStylesToSubtree(UiElement* element, const std::vector<UiCssRule>& cssRules)
    {
        if (element != nullptr)
        {
            ApplyCssRules(element, cssRules);
            ResolveInheritance(element);
            element->RecomputeActiveStyle();
        }
    }

    void UiHtmlParser::ApplyRulesToElement(UiElement* element, const std::vector<UiCssRule>& cssRules)
    {
        for (const UiCssRule& rule : cssRules)
        {
            // Match by tag-based selectors
            bool matches = false;

            // Match by class
            if (!rule.className.empty() && element->HasClass(rule.className))
            {
                matches = true;
            }

            // Match by id (#id)
            if (!rule.selector.empty() && rule.selector[0] == '#')
            {
                std::string ruleId = rule.className; // className stores the id without '#'
                if (element->GetId() == ruleId)
                {
                    matches = true;
                }
            }

            // Match by tag name (element type selectors like "div", "button")
            if (!rule.selector.empty() && rule.selector[0] != '.' && rule.selector[0] != '#')
            {
                std::string baseSelector = rule.className;
                size_t colonPos = baseSelector.find(':');
                if (colonPos != std::string::npos)
                {
                    baseSelector = baseSelector.substr(0, colonPos);
                }

                // Map CSS tag selectors to element types
                bool isTagMatch = false;
                if (baseSelector == "div" && element->GetType() == UiElementType::Panel) isTagMatch = true;
                else if (baseSelector == "button" && element->GetType() == UiElementType::Button) isTagMatch = true;
                else if ((baseSelector == "span" || baseSelector == "p") && element->GetType() == UiElementType::Text) isTagMatch = true;
                else if (baseSelector == "joystick" && element->GetType() == UiElementType::Joystick) isTagMatch = true;
                else if (baseSelector == "*") isTagMatch = true;

                if (isTagMatch)
                {
                    matches = true;
                }
            }

            if (!matches)
            {
                continue;
            }

            // Determine which state to apply the properties to
            int stateIndex = static_cast<int>(UiElementState::Normal);
            if (rule.pseudoClass == "hover")
            {
                stateIndex = static_cast<int>(UiElementState::Hovered);
            }
            else if (rule.pseudoClass == "active")
            {
                stateIndex = static_cast<int>(UiElementState::Pressed);
            }
            else if (rule.pseudoClass == "disabled")
            {
                stateIndex = static_cast<int>(UiElementState::Disabled);
            }

            // Copy properties
            UiStyleProperties& target = element->stateStyles[stateIndex];
            const UiStyleProperties& source = rule.properties;

            // Only copy properties that were explicitly set in the rule
            if (source.display != DisplayType::Flex || source.position != PositionType::Static)
            {
                target.display = source.display;
                target.position = source.position;
            }
            target.flexDirection = source.flexDirection;
            target.alignItems = source.alignItems;
            target.justifyContent = source.justifyContent;
            target.flexWrap = source.flexWrap;
            if (source.gap != 0.0f) target.gap = source.gap;

            if (!source.width.IsAuto()) target.width = source.width;
            if (!source.height.IsAuto()) target.height = source.height;
            if (!source.minWidth.IsAuto()) target.minWidth = source.minWidth;
            if (!source.minHeight.IsAuto()) target.minHeight = source.minHeight;
            if (!source.maxWidth.IsAuto()) target.maxWidth = source.maxWidth;
            if (!source.maxHeight.IsAuto()) target.maxHeight = source.maxHeight;

            if (source.padding.top != 0.0f || source.padding.right != 0.0f ||
                source.padding.bottom != 0.0f || source.padding.left != 0.0f)
            {
                target.padding = source.padding;
            }
            if (source.margin.top != 0.0f || source.margin.right != 0.0f ||
                source.margin.bottom != 0.0f || source.margin.left != 0.0f)
            {
                target.margin = source.margin;
            }

            if (source.top != 0.0f) target.top = source.top;
            if (source.right != 0.0f) target.right = source.right;
            if (source.bottom != 0.0f) target.bottom = source.bottom;
            if (source.left != 0.0f) target.left = source.left;

            if (source.hasBackgroundColor)
            {
                target.backgroundColor = source.backgroundColor;
                target.hasBackgroundColor = true;
            }
            if (source.hasBackgroundImage)
            {
                target.backgroundImage = source.backgroundImage;
                target.backgroundSlice = source.backgroundSlice;
                target.hasBackgroundImage = true;
            }
            if (source.borderWidth != 0.0f)
            {
                target.borderColor = source.borderColor;
                target.borderWidth = source.borderWidth;
            }
            if (source.borderRadius != 0.0f) target.borderRadius = source.borderRadius;
            if (source.opacity != 1.0f) target.opacity = source.opacity;

            if (source.hasFontFamily)
            {
                target.fontFamily = source.fontFamily;
                target.hasFontFamily = true;
            }
            if (source.hasFontSize)
            {
                target.fontSize = source.fontSize;
                target.hasFontSize = true;
            }
            if (source.hasColor)
            {
                target.color = source.color;
                target.hasColor = true;
            }
            if (source.hasTextAlign)
            {
                target.textAlign = source.textAlign;
                target.hasTextAlign = true;
            }
        }

        // Initialize computed style from Normal state
        element->RecomputeActiveStyle();
    }

    void UiHtmlParser::ResolveInheritance(UiElement* element)
    {
        if (element->GetParent())
        {
            const UiStyleProperties& parentStyle = element->GetParent()->computedStyle;
            UiStyleProperties& normalStyle = element->stateStyles[static_cast<int>(UiElementState::Normal)];
            ResolveInheritedProperties(normalStyle, parentStyle);
            element->RecomputeActiveStyle();
        }

        for (UiElement* child : element->GetChildren())
        {
            ResolveInheritance(child);
        }
    }
}
