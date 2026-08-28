#ifndef UIELEMENT_H
#define UIELEMENT_H

#include <string>
#include <vector>
#include "UiStyle.h"
#include "TextRenderer.h"

namespace CC
{
    enum class UiElementState
    {
        Normal,
        Hovered,
        Pressed,
        Disabled,
        StateMax
    };

    enum class UiElementType
    {
        Panel,
        Button,
        Text,
        Slider,
        Toggle,
        Dropdown,
        Joystick
    };

    struct UiRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        bool Contains(float px, float py) const
        {
            return px >= x && px <= x + width && py >= y && py <= y + height;
        }
    };

    class Font;

    struct CachedTextDraw
    {
        std::vector<TextVertex> vertices;
        std::vector<unsigned int> indices;

        // Cache key fields
        std::string text;
        Font* font = nullptr;
        float fontSize = 0.0f;
        float x = 0.0f;
        float y = 0.0f;
        float maxWidth = 0.0f;
        TextAlign alignment = TextAlign::Left;
        bool isValid = false;

        bool Matches(const std::string& _text, Font* _font, float _fontSize,
            float _x, float _y, float _maxWidth, TextAlign _alignment) const
        {
            return isValid && text == _text && font == _font
                && fontSize == _fontSize && x == _x && y == _y
                && maxWidth == _maxWidth && alignment == _alignment;
        }

        void Invalidate()
        {
            isValid = false;
        }
    };

    class UiElement
    {
    public:
        UiElement(UiElementType type);
        virtual ~UiElement();

        // ========================
        // Tree structure
        // ========================

        void AddChild(UiElement* child);
        void ClearChildren();
        const std::vector<UiElement*>& GetChildren() const;
        UiElement* GetParent() const;

        // ========================
        // Identity
        // ========================

        UiElementType GetType() const { return type; }
        const std::string& GetId() const { return id; }
        void SetId(const std::string& _id) { id = _id; }
        const std::vector<std::string>& GetClasses() const { return classes; }
        void AddClass(const std::string& className);
        bool HasClass(const std::string& className) const;
        const std::string& GetDataAction() const { return dataAction; }
        void SetDataAction(const std::string& action) { dataAction = action; }

        // ========================
        // Style
        // ========================

        static constexpr int STATE_COUNT = static_cast<int>(UiElementState::StateMax);
        UiStyleProperties stateStyles[STATE_COUNT];
        UiStyleProperties computedStyle;

        void RecomputeActiveStyle();

        // ========================
        // State
        // ========================

        UiElementState GetState() const { return currentState; }
        void SetState(UiElementState state);
        bool IsEnabled() const { return isEnabled; }
        void SetEnabled(bool enabled);
        bool IsVisible() const { return isVisible; }
        void SetVisible(bool visible) { isVisible = visible; }
        bool IsNavigable() const { return isNavigable; }
        void SetNavigable(bool navigable) { isNavigable = navigable; }

        // ========================
        // Layout
        // ========================

        UiRect layoutRect;
        bool isLayoutDirty = true;

        // Called once per layout pass after the layout engine finishes
        // computing rects for this element's subtree. Lets specialized
        // elements (e.g. UiJoystick) reposition manually-managed children.
        virtual void OnLayoutComputed() {}

        // Measured intrinsic size (from MeasurePass)
        float measuredWidth = 0.0f;
        float measuredHeight = 0.0f;

        // Cached text metrics for render-time vertical centering
        float cachedTextHeight = -1.0f;

        // Cached text vertex data to avoid per-frame LayoutText
        CachedTextDraw cachedTextDraw;

        // ========================
        // Content
        // ========================

        const std::string& GetTextContent() const { return textContent; }
        void SetTextContent(const std::string& text)
        {
            textContent = text;
            cachedTextHeight = -1.0f;
            cachedTextDraw.Invalidate();
        }

    protected:
        UiElementType type;

        // Tree
        UiElement* parent = nullptr;
        std::vector<UiElement*> children;

        // Identity
        std::string id;
        std::vector<std::string> classes;
        std::string dataAction;

        // State
        UiElementState currentState = UiElementState::Normal;
        bool isEnabled = true;
        bool isVisible = true;
        bool isNavigable = false;

        // Content
        std::string textContent;
    };
}

#endif // UIELEMENT_H
