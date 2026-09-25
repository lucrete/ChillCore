#ifndef UITOGGLE_H
#define UITOGGLE_H

#include "UiElement.h"

namespace CC
{
    class UiToggle : public UiElement
    {
    public:
        UiToggle();
        virtual ~UiToggle();

        bool IsChecked() const { return isChecked; }
        void SetChecked(bool checked);
        void Toggle() { SetChecked(!isChecked); }

    private:
        bool isChecked = false;
    };
}

#endif // UITOGGLE_H
