#include "UiText.h"

namespace CC
{
    UiText::UiText(bool _isBlockLevel)
        : UiElement(UiElementType::Text)
        , isBlockLevel(_isBlockLevel)
    {
    }

    UiText::~UiText()
    {
    }
}
