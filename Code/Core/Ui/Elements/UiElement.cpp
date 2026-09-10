#include "UiElement.h"

#include "UiSurface.h"

#include <algorithm>

namespace CC
{
    UiElement::UiElement(UiElementType _type)
        : type(_type)
    {
    }

    UiElement::~UiElement()
    {
        for (UiElement* child : children)
        {
            delete child;
        }
        children.clear();
    }

    // ========================
    // Surface
    // ========================

    void UiElement::MarkSurfaceDirty()
    {
        if (surface != nullptr)
        {
            surface->MarkContentDirty();
        }
    }

    // ========================
    // Tree structure
    // ========================

    void UiElement::AddChild(UiElement* child)
    {
        child->parent = this;
        children.push_back(child);
    }

    void UiElement::ClearChildren()
    {
        for (UiElement* child : children)
        {
            delete child;
        }
        children.clear();
    }

    const std::vector<UiElement*>& UiElement::GetChildren() const
    {
        return children;
    }

    UiElement* UiElement::GetParent() const
    {
        return parent;
    }

    // ========================
    // Identity
    // ========================

    void UiElement::AddClass(const std::string& className)
    {
        classes.push_back(className);
    }

    bool UiElement::HasClass(const std::string& className) const
    {
        return std::find(classes.begin(), classes.end(), className) != classes.end();
    }

    // ========================
    // Style + State
    // ========================

    void UiElement::RecomputeActiveStyle()
    {
        computedStyle = stateStyles[static_cast<int>(UiElementState::Normal)];

        if (currentState != UiElementState::Normal)
        {
            const UiStyleProperties& stateStyle = stateStyles[static_cast<int>(currentState)];

            // Overlay state-specific properties that were explicitly set
            if (stateStyle.hasBackgroundColor)
            {
                computedStyle.backgroundColor = stateStyle.backgroundColor;
                computedStyle.hasBackgroundColor = true;
            }
            if (stateStyle.hasBackgroundImage)
            {
                computedStyle.backgroundImage = stateStyle.backgroundImage;
                computedStyle.backgroundSlice = stateStyle.backgroundSlice;
                computedStyle.hasBackgroundImage = true;
            }
            if (stateStyle.hasColor)
            {
                computedStyle.color = stateStyle.color;
                computedStyle.hasColor = true;
            }
            if (stateStyle.opacity != 1.0f)
            {
                computedStyle.opacity = stateStyle.opacity;
            }
        }
    }

    void UiElement::SetState(UiElementState state)
    {
        if (currentState != state)
        {
            currentState = state;
            RecomputeActiveStyle();
            MarkSurfaceDirty();
        }
    }

    void UiElement::SetVisible(bool visible)
    {
        if (isVisible != visible)
        {
            isVisible = visible;
            MarkSurfaceDirty();
        }
    }

    void UiElement::SetTextContent(const std::string& text)
    {
        if (textContent != text)
        {
            textContent = text;
            cachedTextHeight = -1.0f;
            cachedTextDraw.Invalidate();
            MarkSurfaceDirty();
        }
    }

    void UiElement::SetEnabled(bool enabled)
    {
        isEnabled = enabled;
        if (!enabled)
        {
            SetState(UiElementState::Disabled);
        }
        else if (currentState == UiElementState::Disabled)
        {
            SetState(UiElementState::Normal);
        }
    }
}
