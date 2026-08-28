#include "UiButton.h"

namespace CC
{
    UiButton::UiButton()
        : UiElement(UiElementType::Button)
    {
        isNavigable = true;
    }

    UiButton::~UiButton()
    {
    }
}
