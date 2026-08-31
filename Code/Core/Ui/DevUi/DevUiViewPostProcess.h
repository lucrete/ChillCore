#ifndef DEVUIVIEWPOSTPROCESS_H
#define DEVUIVIEWPOSTPROCESS_H

#include "DevUiView.h"

namespace CC
{
    // Tuning panel for the post-process stack. Sliders are built from each
    // effect's declared parameters and their ranges, so an effect added to
    // PostProcess appears here with no change to this view.
    class DevUiViewPostProcess : public DevUiView
    {
    public:
        void Draw() override;
        const char* GetName() const override;

    private:
        void DrawPresets();
    };
}

#endif // DEVUIVIEWPOSTPROCESS_H
