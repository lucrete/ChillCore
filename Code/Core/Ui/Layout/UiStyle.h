#ifndef UISTYLE_H
#define UISTYLE_H

#include <string>
#include "CCColour.h"

namespace CC
{
    enum class FlexDirection
    {
        Row,
        Column
    };

    enum class AlignItems
    {
        FlexStart,
        FlexEnd,
        Center,
        Stretch
    };

    enum class JustifyContent
    {
        FlexStart,
        FlexEnd,
        Center,
        SpaceBetween,
        SpaceAround,
        SpaceEvenly
    };

    enum class TextAlign
    {
        Left,
        Center,
        Right
    };

    enum class PositionType
    {
        Static,
        Relative,
        Absolute
    };

    enum class DisplayType
    {
        Flex,
        None
    };

    enum class FlexWrap
    {
        NoWrap,
        Wrap
    };

    enum class DimensionUnit
    {
        Px,
        Percent,
        Auto
    };

    struct UiDimension
    {
        float value = 0.0f;
        DimensionUnit unit = DimensionUnit::Auto;

        bool IsAuto() const { return unit == DimensionUnit::Auto; }

        static UiDimension Px(float v) { return {v, DimensionUnit::Px}; }
        static UiDimension Percent(float v) { return {v, DimensionUnit::Percent}; }
        static UiDimension Auto() { return {0.0f, DimensionUnit::Auto}; }
    };

    struct UiEdgeInsets
    {
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        float left = 0.0f;
    };

    struct UiStyleProperties
    {
        // Layout
        DisplayType display = DisplayType::Flex;
        PositionType position = PositionType::Static;
        FlexDirection flexDirection = FlexDirection::Column;
        AlignItems alignItems = AlignItems::Stretch;
        JustifyContent justifyContent = JustifyContent::FlexStart;
        FlexWrap flexWrap = FlexWrap::NoWrap;
        float gap = 0.0f;

        // Sizing
        UiDimension width;
        UiDimension height;
        UiDimension minWidth;
        UiDimension minHeight;
        UiDimension maxWidth;
        UiDimension maxHeight;

        // Spacing
        UiEdgeInsets padding;
        UiEdgeInsets margin;

        // Position offsets (for Relative/Absolute)
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        float left = 0.0f;

        // Appearance
        Colour backgroundColor = Colour::TRANSPARENT();
        Colour borderColor = Colour::TRANSPARENT();
        float borderWidth = 0.0f;
        float borderRadius = 0.0f;
        float opacity = 1.0f;

        // Background image
        std::string backgroundImage;
        float backgroundSlice = 0.0f;

        // Text
        std::string fontFamily;
        float fontSize = 0.0f;
        Colour color = Colour::WHITE();
        TextAlign textAlign = TextAlign::Left;

        // Flags for tracking which properties were explicitly set
        bool hasBackgroundColor = false;
        bool hasBackgroundImage = false;
        bool hasFontFamily = false;
        bool hasFontSize = false;
        bool hasColor = false;
        bool hasTextAlign = false;
    };

    void ResolveInheritedProperties(UiStyleProperties& child, const UiStyleProperties& parent);
    Colour ParseColour(const std::string& value);
    UiDimension ParseDimension(const std::string& value);
}

#endif // UISTYLE_H
