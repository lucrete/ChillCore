#include "UiJoystick.h"

#include "UiPanel.h"

namespace CC
{
    UiJoystick::UiJoystick()
        : UiElement(UiElementType::Joystick)
        , knob(nullptr)
        , knobX(0.0f)
        , knobY(0.0f)
    {
        isNavigable = true;

        // Auto-create knob child. CSS class "joystick-knob" lets the screen
        // style its size and texture without needing the knob in the HTML.
        knob = new UiPanel();
        knob->AddClass("joystick-knob");

        // Force absolute positioning so the layout engine doesn't flow the
        // knob through normal flex layout. The knob's rect is then driven
        // directly by ApplyKnobLayout.
        UiStyleProperties& knobStyle = knob->stateStyles[static_cast<int>(UiElementState::Normal)];
        knobStyle.position = PositionType::Absolute;
        knob->RecomputeActiveStyle();

        AddChild(knob);
    }

    UiJoystick::~UiJoystick()
    {
    }

    void UiJoystick::SetKnobNormalized(float x, float y)
    {
        knobX = x;
        knobY = y;
        ApplyKnobLayout();
    }

    void UiJoystick::OnLayoutComputed()
    {
        ApplyKnobLayout();
    }

    void UiJoystick::ApplyKnobLayout()
    {
        if (knob != nullptr)
        {
            float outerHalfWidth = layoutRect.width * 0.5f;
            float outerHalfHeight = layoutRect.height * 0.5f;
            float radius = outerHalfWidth < outerHalfHeight ? outerHalfWidth : outerHalfHeight;

            float centerX = layoutRect.x + outerHalfWidth;
            float centerY = layoutRect.y + outerHalfHeight;

            float knobHalfWidth = knob->layoutRect.width * 0.5f;
            float knobHalfHeight = knob->layoutRect.height * 0.5f;

            // knobY uses analog-stick convention (up = +y); screen-y is
            // inverted, so subtract when projecting to layoutRect.
            knob->layoutRect.x = centerX + knobX * radius - knobHalfWidth;
            knob->layoutRect.y = centerY - knobY * radius - knobHalfHeight;
        }
    }
}
