#include "UiLayoutEngine.h"

#include "UiElement.h"
#include "TextRenderer.h"
#include "FontManager.h"
#include "Font.h"

#include <algorithm>

namespace CC
{
    float UiLayoutEngine::scaleFactor = 1.0f;

    void UiLayoutEngine::ComputeLayout(UiElement* root, int screenWidth, int screenHeight)
    {
        scaleFactor = screenHeight / 1080.0f;

        float sw = (float)screenWidth;
        float sh = (float)screenHeight;

        // Root fills the screen
        root->layoutRect.x = 0.0f;
        root->layoutRect.y = 0.0f;
        root->layoutRect.width = sw;
        root->layoutRect.height = sh;

        InvalidateTextCaches(root);
        MeasurePass(root, sw, sh);
        ArrangePass(root, 0.0f, 0.0f, sw, sh);
    }

    // ========================
    // Measure Pass (bottom-up)
    // ========================

    void UiLayoutEngine::MeasurePass(UiElement* element, float parentWidth, float parentHeight)
    {
        const UiStyleProperties& style = element->computedStyle;

        // Resolve this element's width first so children know their available space
        float resolvedWidth = ResolveWidth(element, parentWidth);

        // Determine the available width for children:
        // Start from the element's own width if known, otherwise the parent's available width.
        // Always subtract this element's padding to get the content area.
        float elementWidth = resolvedWidth > 0.0f ? resolvedWidth : parentWidth;
        float childAvailableWidth = elementWidth - (style.padding.left + style.padding.right) * scaleFactor;
        if (childAvailableWidth < 0.0f) childAvailableWidth = 0.0f;

        // Recurse children with the correct available width
        for (UiElement* child : element->GetChildren())
        {
            MeasurePass(child, childAvailableWidth, parentHeight);
        }

        // Resolve explicit height
        float resolvedHeight = ResolveHeight(element, parentHeight);

        // If width is auto, compute from content
        if (resolvedWidth < 0.0f)
        {
            // Text content width
            if (!element->GetTextContent().empty() && style.hasFontSize)
            {
                Font* font = FontManager::Get()->GetFont(
                    style.hasFontFamily ? style.fontFamily : "robotoRegular");
                if (font)
                {
                    float fontSize = style.fontSize * scaleFactor;
                    // Measure text within available parent width for wrapping
                    float maxWidth = parentWidth - (style.padding.left + style.padding.right) * scaleFactor;
                    if (maxWidth < 0.0f) maxWidth = 0.0f;
                    TextMetrics metrics = TextRenderer::Get()->MeasureText(
                        element->GetTextContent(), font, fontSize, maxWidth);
                    resolvedWidth = metrics.width + (style.padding.left + style.padding.right) * scaleFactor;
                }
            }

            // Sum of children
            if (resolvedWidth < 0.0f && !element->GetChildren().empty())
            {
                float childSum = 0.0f;
                float childMax = 0.0f;
                int visibleChildCount = 0;

                for (UiElement* child : element->GetChildren())
                {
                    if (!child->IsVisible() || child->computedStyle.display == DisplayType::None)
                    {
                        continue;
                    }
                    if (child->computedStyle.position == PositionType::Absolute)
                    {
                        continue;
                    }
                    float cw = child->measuredWidth +
                        (child->computedStyle.margin.left + child->computedStyle.margin.right) * scaleFactor;
                    childSum += cw;
                    if (cw > childMax) childMax = cw;
                    visibleChildCount++;
                }

                float gapTotal = (visibleChildCount > 1) ? style.gap * scaleFactor * (visibleChildCount - 1) : 0.0f;

                if (style.flexDirection == FlexDirection::Row)
                {
                    resolvedWidth = childSum + gapTotal + (style.padding.left + style.padding.right) * scaleFactor;
                }
                else
                {
                    resolvedWidth = childMax + (style.padding.left + style.padding.right) * scaleFactor;
                }
            }

            if (resolvedWidth < 0.0f)
            {
                resolvedWidth = 0.0f;
            }
        }

        if (resolvedHeight < 0.0f)
        {
            // Text content height
            if (!element->GetTextContent().empty() && style.hasFontSize)
            {
                Font* font = FontManager::Get()->GetFont(
                    style.hasFontFamily ? style.fontFamily : "robotoRegular");
                if (font)
                {
                    float fontSize = style.fontSize * scaleFactor;
                    float maxWidth = parentWidth - (style.padding.left + style.padding.right) * scaleFactor;
                    if (maxWidth < 0.0f) maxWidth = 0.0f;
                    TextMetrics metrics = TextRenderer::Get()->MeasureText(
                        element->GetTextContent(), font, fontSize, maxWidth);
                    resolvedHeight = metrics.height + (style.padding.top + style.padding.bottom) * scaleFactor;
                }
            }

            // Sum of children
            if (resolvedHeight < 0.0f && !element->GetChildren().empty())
            {
                float childSum = 0.0f;
                float childMax = 0.0f;
                int visibleChildCount = 0;

                for (UiElement* child : element->GetChildren())
                {
                    if (!child->IsVisible() || child->computedStyle.display == DisplayType::None)
                    {
                        continue;
                    }
                    if (child->computedStyle.position == PositionType::Absolute)
                    {
                        continue;
                    }
                    float ch = child->measuredHeight +
                        (child->computedStyle.margin.top + child->computedStyle.margin.bottom) * scaleFactor;
                    childSum += ch;
                    if (ch > childMax) childMax = ch;
                    visibleChildCount++;
                }

                float gapTotal = (visibleChildCount > 1) ? style.gap * scaleFactor * (visibleChildCount - 1) : 0.0f;

                if (style.flexDirection == FlexDirection::Column)
                {
                    resolvedHeight = childSum + gapTotal + (style.padding.top + style.padding.bottom) * scaleFactor;
                }
                else
                {
                    resolvedHeight = childMax + (style.padding.top + style.padding.bottom) * scaleFactor;
                }
            }

            if (resolvedHeight < 0.0f)
            {
                resolvedHeight = 0.0f;
            }
        }

        // Apply min/max constraints
        resolvedWidth = ClampDimension(resolvedWidth,
            style.minWidth.value, style.maxWidth.value, parentWidth,
            style.minWidth.IsAuto(), style.maxWidth.IsAuto());
        resolvedHeight = ClampDimension(resolvedHeight,
            style.minHeight.value, style.maxHeight.value, parentHeight,
            style.minHeight.IsAuto(), style.maxHeight.IsAuto());

        element->measuredWidth = resolvedWidth;
        element->measuredHeight = resolvedHeight;
    }

    // ========================
    // Arrange Pass (top-down)
    // ========================

    void UiLayoutEngine::ArrangePass(UiElement* element, float parentX, float parentY,
        float parentWidth, float parentHeight)
    {
        const UiStyleProperties& style = element->computedStyle;

        float elemWidth = element->measuredWidth;
        float elemHeight = element->measuredHeight;

        // Stretch to parent for root or Stretch-aligned children
        if (element->GetParent() == nullptr)
        {
            elemWidth = parentWidth;
            elemHeight = parentHeight;
        }

        element->layoutRect.width = elemWidth;
        element->layoutRect.height = elemHeight;

        // Content area (inside padding)
        float contentX = element->layoutRect.x + style.padding.left * scaleFactor;
        float contentY = element->layoutRect.y + style.padding.top * scaleFactor;
        float contentWidth = elemWidth - (style.padding.left + style.padding.right) * scaleFactor;
        float contentHeight = elemHeight - (style.padding.top + style.padding.bottom) * scaleFactor;

        if (contentWidth < 0.0f) contentWidth = 0.0f;
        if (contentHeight < 0.0f) contentHeight = 0.0f;

        // Collect visible, non-absolute children
        std::vector<UiElement*> flowChildren;
        std::vector<UiElement*> absoluteChildren;

        for (UiElement* child : element->GetChildren())
        {
            if (!child->IsVisible() || child->computedStyle.display == DisplayType::None)
            {
                continue;
            }
            if (child->computedStyle.position == PositionType::Absolute)
            {
                absoluteChildren.push_back(child);
            }
            else
            {
                flowChildren.push_back(child);
            }
        }

        // ========================
        // Flex layout for flow children
        // ========================

        bool isRow = (style.flexDirection == FlexDirection::Row);
        float mainAxisSize = isRow ? contentWidth : contentHeight;
        float crossAxisSize = isRow ? contentHeight : contentWidth;

        // Calculate total child size along main axis
        float totalMainSize = 0.0f;
        for (UiElement* child : flowChildren)
        {
            float margin = isRow ?
                (child->computedStyle.margin.left + child->computedStyle.margin.right) * scaleFactor :
                (child->computedStyle.margin.top + child->computedStyle.margin.bottom) * scaleFactor;
            float childMain = isRow ? child->measuredWidth : child->measuredHeight;
            totalMainSize += childMain + margin;
        }

        float gapTotal = (flowChildren.size() > 1) ? style.gap * scaleFactor * ((float)flowChildren.size() - 1) : 0.0f;
        totalMainSize += gapTotal;

        float freeSpace = mainAxisSize - totalMainSize;
        if (freeSpace < 0.0f) freeSpace = 0.0f;

        // Compute spacing from justify-content
        float mainOffset = 0.0f;
        float mainSpacing = style.gap * scaleFactor;

        int childCount = (int)flowChildren.size();
        if (childCount > 0)
        {
            switch (style.justifyContent)
            {
            case JustifyContent::FlexStart:
                mainOffset = 0.0f;
                break;
            case JustifyContent::FlexEnd:
                mainOffset = freeSpace;
                break;
            case JustifyContent::Center:
                mainOffset = freeSpace * 0.5f;
                break;
            case JustifyContent::SpaceBetween:
                mainOffset = 0.0f;
                if (childCount > 1)
                {
                    mainSpacing = (mainAxisSize - (totalMainSize - gapTotal)) / (float)(childCount - 1);
                }
                break;
            case JustifyContent::SpaceAround:
                if (childCount > 0)
                {
                    float spacing = (mainAxisSize - (totalMainSize - gapTotal)) / (float)childCount;
                    mainOffset = spacing * 0.5f;
                    mainSpacing = spacing;
                }
                break;
            case JustifyContent::SpaceEvenly:
                if (childCount > 0)
                {
                    float spacing = (mainAxisSize - (totalMainSize - gapTotal)) / (float)(childCount + 1);
                    mainOffset = spacing;
                    mainSpacing = spacing;
                }
                break;
            }
        }

        // Position each child
        float cursor = mainOffset;
        for (int i = 0; i < childCount; i++)
        {
            UiElement* child = flowChildren[i];
            const UiStyleProperties& childStyle = child->computedStyle;

            float childMainSize = isRow ? child->measuredWidth : child->measuredHeight;
            float childCrossSize = isRow ? child->measuredHeight : child->measuredWidth;

            float marginMainBefore = isRow ? childStyle.margin.left * scaleFactor : childStyle.margin.top * scaleFactor;
            float marginMainAfter = isRow ? childStyle.margin.right * scaleFactor : childStyle.margin.bottom * scaleFactor;
            float marginCrossBefore = isRow ? childStyle.margin.top * scaleFactor : childStyle.margin.left * scaleFactor;

            cursor += marginMainBefore;

            // Cross-axis positioning
            float crossOffset = marginCrossBefore;
            float effectiveCrossSize = childCrossSize;

            switch (style.alignItems)
            {
            case AlignItems::FlexStart:
                crossOffset = marginCrossBefore;
                break;
            case AlignItems::FlexEnd:
                crossOffset = crossAxisSize - childCrossSize - marginCrossBefore;
                break;
            case AlignItems::Center:
                crossOffset = (crossAxisSize - childCrossSize) * 0.5f;
                break;
            case AlignItems::Stretch:
            {
                float marginCrossAfter = isRow ? childStyle.margin.bottom * scaleFactor : childStyle.margin.right * scaleFactor;
                effectiveCrossSize = crossAxisSize - marginCrossBefore - marginCrossAfter;
                if (effectiveCrossSize < 0.0f) effectiveCrossSize = 0.0f;

                // Only stretch if dimension is auto
                bool crossIsAuto = isRow ? childStyle.height.IsAuto() : childStyle.width.IsAuto();
                if (crossIsAuto)
                {
                    if (isRow)
                    {
                        child->measuredHeight = effectiveCrossSize;
                    }
                    else
                    {
                        child->measuredWidth = effectiveCrossSize;
                    }
                }
                else
                {
                    effectiveCrossSize = childCrossSize;
                }
                crossOffset = marginCrossBefore;
                break;
            }
            }

            // Set position
            if (isRow)
            {
                child->layoutRect.x = contentX + cursor;
                child->layoutRect.y = contentY + crossOffset;
                child->layoutRect.width = child->measuredWidth;
                child->layoutRect.height = isRow && style.alignItems == AlignItems::Stretch && childStyle.height.IsAuto()
                    ? effectiveCrossSize : child->measuredHeight;
            }
            else
            {
                child->layoutRect.x = contentX + crossOffset;
                child->layoutRect.y = contentY + cursor;
                child->layoutRect.width = !isRow && style.alignItems == AlignItems::Stretch && childStyle.width.IsAuto()
                    ? effectiveCrossSize : child->measuredWidth;
                child->layoutRect.height = child->measuredHeight;
            }

            cursor += childMainSize + marginMainAfter + mainSpacing;

            // Apply relative offsets
            if (childStyle.position == PositionType::Relative)
            {
                child->layoutRect.x += childStyle.left * scaleFactor;
                child->layoutRect.y += childStyle.top * scaleFactor;
            }

            // Recurse into child
            ArrangePass(child, child->layoutRect.x, child->layoutRect.y,
                child->layoutRect.width, child->layoutRect.height);
        }

        // ========================
        // Absolute-positioned children
        // ========================

        for (UiElement* child : absoluteChildren)
        {
            const UiStyleProperties& childStyle = child->computedStyle;

            child->layoutRect.width = child->measuredWidth;
            child->layoutRect.height = child->measuredHeight;

            // Anchor: prefer right when set (and left isn't), else use left.
            if (childStyle.right != 0.0f && childStyle.left == 0.0f)
            {
                child->layoutRect.x = element->layoutRect.x + element->layoutRect.width
                    - child->layoutRect.width - childStyle.right * scaleFactor;
            }
            else
            {
                child->layoutRect.x = element->layoutRect.x + childStyle.left * scaleFactor;
            }

            if (childStyle.bottom != 0.0f && childStyle.top == 0.0f)
            {
                child->layoutRect.y = element->layoutRect.y + element->layoutRect.height
                    - child->layoutRect.height - childStyle.bottom * scaleFactor;
            }
            else
            {
                child->layoutRect.y = element->layoutRect.y + childStyle.top * scaleFactor;
            }

            ArrangePass(child, child->layoutRect.x, child->layoutRect.y,
                child->layoutRect.width, child->layoutRect.height);
        }

        element->isLayoutDirty = false;

        // Per-element hook for elements that manage children manually
        // (e.g. UiJoystick repositions its knob to track normalized state).
        element->OnLayoutComputed();
    }

    // ========================
    // Cache invalidation
    // ========================

    void UiLayoutEngine::InvalidateTextCaches(UiElement* element)
    {
        element->cachedTextHeight = -1.0f;
        element->cachedTextDraw.Invalidate();
        for (UiElement* child : element->GetChildren())
        {
            InvalidateTextCaches(child);
        }
    }

    // ========================
    // Dimension resolution
    // ========================

    float UiLayoutEngine::ResolveWidth(UiElement* element, float parentWidth)
    {
        const UiDimension& dim = element->computedStyle.width;
        if (dim.IsAuto())
        {
            return -1.0f; // Signal: compute from content
        }
        if (dim.unit == DimensionUnit::Percent)
        {
            return parentWidth * dim.value / 100.0f;
        }
        return dim.value * scaleFactor;
    }

    float UiLayoutEngine::ResolveHeight(UiElement* element, float parentHeight)
    {
        const UiDimension& dim = element->computedStyle.height;
        if (dim.IsAuto())
        {
            return -1.0f; // Signal: compute from content
        }
        if (dim.unit == DimensionUnit::Percent)
        {
            return parentHeight * dim.value / 100.0f;
        }
        return dim.value * scaleFactor;
    }

    float UiLayoutEngine::ClampDimension(float value, float minVal, float maxVal,
        float parentSize, bool isMinAuto, bool isMaxAuto)
    {
        float result = value;
        if (!isMinAuto)
        {
            float resolvedMin = minVal * scaleFactor;
            if (result < resolvedMin) result = resolvedMin;
        }
        if (!isMaxAuto)
        {
            float resolvedMax = maxVal * scaleFactor;
            if (result > resolvedMax) result = resolvedMax;
        }
        return result;
    }
}
