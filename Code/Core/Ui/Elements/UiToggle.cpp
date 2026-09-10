#include "UiToggle.h"

namespace CC
{
    UiToggle::UiToggle()
        : UiElement(UiElementType::Toggle)
    {
        isNavigable = true;
    }

    UiToggle::~UiToggle()
    {
    }

    void UiToggle::SetChecked(bool checked)
    {
        if (isChecked != checked)
        {
            isChecked = checked;
            MarkSurfaceDirty();
        }
    }
}
