#include "UiCssParser.h"

#include <cstdlib>
#include <sstream>

namespace CC
{
    std::string UiCssParser::Trim(const std::string& str)
    {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
        {
            return "";
        }
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    std::vector<UiCssRule> UiCssParser::Parse(const std::string& cssText)
    {
        std::vector<UiCssRule> rules;
        size_t pos = 0;
        size_t length = cssText.length();

        while (pos < length)
        {
            // Skip whitespace
            while (pos < length && (cssText[pos] == ' ' || cssText[pos] == '\t' ||
                cssText[pos] == '\r' || cssText[pos] == '\n'))
            {
                pos++;
            }

            // Skip comments
            if (pos + 1 < length && cssText[pos] == '/' && cssText[pos + 1] == '*')
            {
                pos += 2;
                while (pos + 1 < length && !(cssText[pos] == '*' && cssText[pos + 1] == '/'))
                {
                    pos++;
                }
                if (pos + 1 < length)
                {
                    pos += 2;
                }
                continue;
            }

            if (pos >= length)
            {
                break;
            }

            // Read selector (up to '{')
            size_t selectorStart = pos;
            while (pos < length && cssText[pos] != '{')
            {
                pos++;
            }
            if (pos >= length)
            {
                break;
            }

            std::string selector = Trim(cssText.substr(selectorStart, pos - selectorStart));
            pos++; // skip '{'

            // Read declarations (up to '}')
            size_t blockStart = pos;
            int braceDepth = 1;
            while (pos < length && braceDepth > 0)
            {
                if (cssText[pos] == '{')
                {
                    braceDepth++;
                }
                else if (cssText[pos] == '}')
                {
                    braceDepth--;
                }
                if (braceDepth > 0)
                {
                    pos++;
                }
            }

            std::string block = cssText.substr(blockStart, pos - blockStart);
            if (pos < length)
            {
                pos++; // skip '}'
            }

            // Parse selector into className and pseudoClass
            UiCssRule rule;
            rule.selector = selector;

            // Check for pseudo-class
            size_t colonPos = selector.find(':');
            std::string baseSelector;
            if (colonPos != std::string::npos)
            {
                rule.pseudoClass = selector.substr(colonPos + 1);
                baseSelector = selector.substr(0, colonPos);
            }
            else
            {
                baseSelector = selector;
            }

            // Strip leading '.' for class selectors
            if (!baseSelector.empty() && baseSelector[0] == '.')
            {
                rule.className = baseSelector.substr(1);
            }
            else
            {
                rule.className = baseSelector;
            }

            // Parse property declarations
            size_t declPos = 0;
            while (declPos < block.length())
            {
                // Skip whitespace
                while (declPos < block.length() && (block[declPos] == ' ' || block[declPos] == '\t' ||
                    block[declPos] == '\r' || block[declPos] == '\n'))
                {
                    declPos++;
                }

                if (declPos >= block.length())
                {
                    break;
                }

                // Read property name (up to ':')
                size_t nameStart = declPos;
                while (declPos < block.length() && block[declPos] != ':' && block[declPos] != ';')
                {
                    declPos++;
                }

                if (declPos >= block.length() || block[declPos] == ';')
                {
                    declPos++;
                    continue;
                }

                std::string propName = Trim(block.substr(nameStart, declPos - nameStart));
                declPos++; // skip ':'

                // Read property value (up to ';' or end of block)
                // Handle parentheses for rgba() etc.
                size_t valueStart = declPos;
                int parenDepth = 0;
                while (declPos < block.length())
                {
                    if (block[declPos] == '(')
                    {
                        parenDepth++;
                    }
                    else if (block[declPos] == ')')
                    {
                        parenDepth--;
                    }
                    else if (block[declPos] == ';' && parenDepth == 0)
                    {
                        break;
                    }
                    declPos++;
                }

                std::string propValue = Trim(block.substr(valueStart, declPos - valueStart));
                if (declPos < block.length())
                {
                    declPos++; // skip ';'
                }

                if (!propName.empty() && !propValue.empty())
                {
                    ApplyProperty(rule.properties, propName, propValue);
                }
            }

            rules.push_back(rule);
        }

        return rules;
    }

    void UiCssParser::ParseEdgeShorthand(const std::string& value, UiEdgeInsets& edges)
    {
        std::vector<float> values;
        std::istringstream stream(value);
        std::string token;

        while (stream >> token)
        {
            float v = std::strtof(token.c_str(), nullptr);
            values.push_back(v);
        }

        if (values.size() == 1)
        {
            edges.top = values[0];
            edges.right = values[0];
            edges.bottom = values[0];
            edges.left = values[0];
        }
        else if (values.size() == 2)
        {
            edges.top = values[0];
            edges.bottom = values[0];
            edges.right = values[1];
            edges.left = values[1];
        }
        else if (values.size() == 3)
        {
            edges.top = values[0];
            edges.right = values[1];
            edges.left = values[1];
            edges.bottom = values[2];
        }
        else if (values.size() >= 4)
        {
            edges.top = values[0];
            edges.right = values[1];
            edges.bottom = values[2];
            edges.left = values[3];
        }
    }

    void UiCssParser::ApplyProperty(UiStyleProperties& properties, const std::string& name, const std::string& value)
    {
        // ========================
        // Layout properties
        // ========================

        if (name == "display")
        {
            if (value == "flex") properties.display = DisplayType::Flex;
            else if (value == "none") properties.display = DisplayType::None;
        }
        else if (name == "position")
        {
            if (value == "static") properties.position = PositionType::Static;
            else if (value == "relative") properties.position = PositionType::Relative;
            else if (value == "absolute") properties.position = PositionType::Absolute;
        }
        else if (name == "flex-direction")
        {
            if (value == "row") properties.flexDirection = FlexDirection::Row;
            else if (value == "column") properties.flexDirection = FlexDirection::Column;
        }
        else if (name == "align-items")
        {
            if (value == "flex-start") properties.alignItems = AlignItems::FlexStart;
            else if (value == "flex-end") properties.alignItems = AlignItems::FlexEnd;
            else if (value == "center") properties.alignItems = AlignItems::Center;
            else if (value == "stretch") properties.alignItems = AlignItems::Stretch;
        }
        else if (name == "justify-content")
        {
            if (value == "flex-start") properties.justifyContent = JustifyContent::FlexStart;
            else if (value == "flex-end") properties.justifyContent = JustifyContent::FlexEnd;
            else if (value == "center") properties.justifyContent = JustifyContent::Center;
            else if (value == "space-between") properties.justifyContent = JustifyContent::SpaceBetween;
            else if (value == "space-around") properties.justifyContent = JustifyContent::SpaceAround;
            else if (value == "space-evenly") properties.justifyContent = JustifyContent::SpaceEvenly;
        }
        else if (name == "flex-wrap")
        {
            if (value == "nowrap") properties.flexWrap = FlexWrap::NoWrap;
            else if (value == "wrap") properties.flexWrap = FlexWrap::Wrap;
        }
        else if (name == "gap")
        {
            properties.gap = std::strtof(value.c_str(), nullptr);
        }

        // ========================
        // Sizing properties
        // ========================

        else if (name == "width")
        {
            properties.width = ParseDimension(value);
        }
        else if (name == "height")
        {
            properties.height = ParseDimension(value);
        }
        else if (name == "min-width")
        {
            properties.minWidth = ParseDimension(value);
        }
        else if (name == "min-height")
        {
            properties.minHeight = ParseDimension(value);
        }
        else if (name == "max-width")
        {
            properties.maxWidth = ParseDimension(value);
        }
        else if (name == "max-height")
        {
            properties.maxHeight = ParseDimension(value);
        }

        // ========================
        // Spacing properties
        // ========================

        else if (name == "padding")
        {
            ParseEdgeShorthand(value, properties.padding);
        }
        else if (name == "padding-top")
        {
            properties.padding.top = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "padding-right")
        {
            properties.padding.right = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "padding-bottom")
        {
            properties.padding.bottom = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "padding-left")
        {
            properties.padding.left = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "margin")
        {
            ParseEdgeShorthand(value, properties.margin);
        }
        else if (name == "margin-top")
        {
            properties.margin.top = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "margin-right")
        {
            properties.margin.right = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "margin-bottom")
        {
            properties.margin.bottom = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "margin-left")
        {
            properties.margin.left = std::strtof(value.c_str(), nullptr);
        }

        // ========================
        // Position offsets
        // ========================

        else if (name == "top")
        {
            properties.top = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "right")
        {
            properties.right = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "bottom")
        {
            properties.bottom = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "left")
        {
            properties.left = std::strtof(value.c_str(), nullptr);
        }

        // ========================
        // Appearance properties
        // ========================

        else if (name == "background-color" || name == "background")
        {
            properties.backgroundColor = ParseColour(value);
            properties.hasBackgroundColor = true;
        }
        else if (name == "border-color")
        {
            properties.borderColor = ParseColour(value);
        }
        else if (name == "border-width")
        {
            properties.borderWidth = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "border-radius")
        {
            properties.borderRadius = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "background-image")
        {
            properties.backgroundImage = value;
            properties.hasBackgroundImage = true;
        }
        else if (name == "background-slice")
        {
            properties.backgroundSlice = std::strtof(value.c_str(), nullptr);
        }
        else if (name == "opacity")
        {
            properties.opacity = std::strtof(value.c_str(), nullptr);
        }

        // ========================
        // Text properties
        // ========================

        else if (name == "font-family")
        {
            properties.fontFamily = value;
            properties.hasFontFamily = true;
        }
        else if (name == "font-size")
        {
            properties.fontSize = std::strtof(value.c_str(), nullptr);
            properties.hasFontSize = true;
        }
        else if (name == "color")
        {
            properties.color = ParseColour(value);
            properties.hasColor = true;
        }
        else if (name == "text-align")
        {
            if (value == "left") properties.textAlign = TextAlign::Left;
            else if (value == "center") properties.textAlign = TextAlign::Center;
            else if (value == "right") properties.textAlign = TextAlign::Right;
            properties.hasTextAlign = true;
        }
    }
}
