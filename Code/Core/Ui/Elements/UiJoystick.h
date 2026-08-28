#ifndef UIJOYSTICK_H
#define UIJOYSTICK_H

#include "UiElement.h"

namespace CC
{
    // On-screen joystick. The element itself is the outer ring; an
    // auto-created child UiPanel is the knob, positioned via direct
    // layoutRect writes (see OnLayoutComputed and SetKnobNormalized).
    class UiJoystick : public UiElement
    {
    public:
        UiJoystick();
        virtual ~UiJoystick();

        // Knob position normalized to the outer radius. Range: [-1, 1].
        // Magnitude is clamped to 1 by callers.
        float GetKnobX() const { return knobX; }
        float GetKnobY() const { return knobY; }
        void SetKnobNormalized(float x, float y);

        UiElement* GetKnob() const { return knob; }

        // Reposition the knob according to current normalized values.
        // Called after the layout engine has computed this joystick's rect.
        virtual void OnLayoutComputed() override;

    private:
        UiElement* knob;
        float knobX;
        float knobY;

        void ApplyKnobLayout();
    };
}

#endif // UIJOYSTICK_H
