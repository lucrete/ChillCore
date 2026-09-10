#include "UiSlider.h"

#include <algorithm>

namespace CC
{
    UiSlider::UiSlider()
        : UiElement(UiElementType::Slider)
    {
        isNavigable = true;
    }

    UiSlider::~UiSlider()
    {
    }

    void UiSlider::SetCurrentValue(float value)
    {
        float clamped = std::max(minValue, std::min(maxValue, value));
        if (clamped != currentValue)
        {
            currentValue = clamped;
            MarkSurfaceDirty();
        }
    }

    float UiSlider::GetNormalizedValue() const
    {
        if (maxValue <= minValue)
        {
            return 0.0f;
        }
        return (currentValue - minValue) / (maxValue - minValue);
    }
}
