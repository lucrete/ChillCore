#include "UiStyle.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace CC
{
    void ResolveInheritedProperties(UiStyleProperties& child, const UiStyleProperties& parent)
    {
        if (!child.hasFontFamily && parent.hasFontFamily)
        {
            child.fontFamily = parent.fontFamily;
            child.hasFontFamily = true;
        }
        if (!child.hasFontSize && parent.hasFontSize)
        {
            child.fontSize = parent.fontSize;
            child.hasFontSize = true;
        }
        if (!child.hasColor && parent.hasColor)
        {
            child.color = parent.color;
            child.hasColor = true;
        }
        if (!child.hasTextAlign && parent.hasTextAlign)
        {
            child.textAlign = parent.textAlign;
            child.hasTextAlign = true;
        }
    }

    // ========================
    // Colour parsing
    // ========================

    static int HexCharToInt(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    }

    static float HexByteToFloat(char high, char low)
    {
        return (HexCharToInt(high) * 16 + HexCharToInt(low)) / 255.0f;
    }

    Colour ParseColour(const std::string& value)
    {
        if (value.empty())
        {
            return Colour::TRANSPARENT();
        }

        // Hex colour: #RGB, #RRGGBB, #RRGGBBAA
        if (value[0] == '#')
        {
            if (value.length() == 4)
            {
                float r = HexCharToInt(value[1]) / 15.0f;
                float g = HexCharToInt(value[2]) / 15.0f;
                float b = HexCharToInt(value[3]) / 15.0f;
                return Colour(r, g, b, 1.0f);
            }
            if (value.length() == 7)
            {
                float r = HexByteToFloat(value[1], value[2]);
                float g = HexByteToFloat(value[3], value[4]);
                float b = HexByteToFloat(value[5], value[6]);
                return Colour(r, g, b, 1.0f);
            }
            if (value.length() == 9)
            {
                float r = HexByteToFloat(value[1], value[2]);
                float g = HexByteToFloat(value[3], value[4]);
                float b = HexByteToFloat(value[5], value[6]);
                float a = HexByteToFloat(value[7], value[8]);
                return Colour(r, g, b, a);
            }
        }

        // rgba(r, g, b, a) format
        if (value.substr(0, 5) == "rgba(")
        {
            size_t start = 5;
            size_t end = value.find(')', start);
            if (end == std::string::npos)
            {
                return Colour::TRANSPARENT();
            }
            std::string inner = value.substr(start, end - start);

            float components[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            int componentIndex = 0;
            size_t pos = 0;

            while (pos < inner.length() && componentIndex < 4)
            {
                while (pos < inner.length() && (inner[pos] == ' ' || inner[pos] == ','))
                {
                    pos++;
                }
                if (pos >= inner.length())
                {
                    break;
                }
                components[componentIndex] = std::strtof(inner.c_str() + pos, nullptr);

                // Values 0-255 for rgb, 0-1 for alpha
                if (componentIndex < 3 && components[componentIndex] > 1.0f)
                {
                    components[componentIndex] /= 255.0f;
                }
                componentIndex++;

                while (pos < inner.length() && inner[pos] != ',' && inner[pos] != ' ')
                {
                    pos++;
                }
            }

            return Colour(components[0], components[1], components[2], components[3]);
        }

        // rgb(r, g, b) format
        if (value.substr(0, 4) == "rgb(")
        {
            size_t start = 4;
            size_t end = value.find(')', start);
            if (end == std::string::npos)
            {
                return Colour::TRANSPARENT();
            }
            std::string inner = value.substr(start, end - start);

            float components[3] = {0.0f, 0.0f, 0.0f};
            int componentIndex = 0;
            size_t pos = 0;

            while (pos < inner.length() && componentIndex < 3)
            {
                while (pos < inner.length() && (inner[pos] == ' ' || inner[pos] == ','))
                {
                    pos++;
                }
                if (pos >= inner.length())
                {
                    break;
                }
                components[componentIndex] = std::strtof(inner.c_str() + pos, nullptr);
                if (components[componentIndex] > 1.0f)
                {
                    components[componentIndex] /= 255.0f;
                }
                componentIndex++;

                while (pos < inner.length() && inner[pos] != ',' && inner[pos] != ' ')
                {
                    pos++;
                }
            }

            return Colour(components[0], components[1], components[2], 1.0f);
        }

        // Named colours
        if (value == "transparent") return Colour::TRANSPARENT();
        if (value == "white") return Colour::WHITE();
        if (value == "black") return Colour::BLACK();
        if (value == "red") return Colour::RED();
        if (value == "green") return Colour::GREEN();
        if (value == "blue") return Colour::BLUE();
        if (value == "yellow") return Colour::YELLOW();

        return Colour::TRANSPARENT();
    }

    // ========================
    // Dimension parsing
    // ========================

    UiDimension ParseDimension(const std::string& value)
    {
        if (value.empty() || value == "auto")
        {
            return UiDimension::Auto();
        }

        if (value.length() > 1 && value.back() == '%')
        {
            float v = std::strtof(value.c_str(), nullptr);
            return UiDimension::Percent(v);
        }

        // Parse numeric value, strip "px" suffix if present
        float v = std::strtof(value.c_str(), nullptr);
        return UiDimension::Px(v);
    }
}
