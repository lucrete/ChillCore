#ifndef DEVUIVIEWABOUT_H
#define DEVUIVIEWABOUT_H

#include "DevUiView.h"

namespace CC
{
    class DevUiViewAbout : public DevUiView
    {
    public:
        void Draw() override;
        const char* GetName() const override;
    };
}

#endif // DEVUIVIEWABOUT_H
