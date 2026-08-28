#ifndef UISLIDER_H
#define UISLIDER_H

#include "UiElement.h"

namespace CC
{
    class UiSlider : public UiElement
    {
    public:
        UiSlider();
        virtual ~UiSlider();

        float GetMinValue() const { return minValue; }
        void SetMinValue(float value) { minValue = value; }
        float GetMaxValue() const { return maxValue; }
        void SetMaxValue(float value) { maxValue = value; }
        float GetCurrentValue() const { return currentValue; }
        void SetCurrentValue(float value);

        float GetNormalizedValue() const;

    private:
        float minValue = 0.0f;
        float maxValue = 100.0f;
        float currentValue = 50.0f;
    };
}

#endif // UISLIDER_H
