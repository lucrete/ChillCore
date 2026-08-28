#ifndef UITEXT_H
#define UITEXT_H

#include "UiElement.h"

namespace CC
{
    class UiText : public UiElement
    {
    public:
        UiText(bool _isBlockLevel = false);
        virtual ~UiText();

        bool IsBlockLevel() const { return isBlockLevel; }

    private:
        bool isBlockLevel;
    };
}

#endif // UITEXT_H
