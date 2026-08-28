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
}
