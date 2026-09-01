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
        void DrawPresetSaving();

        // Name for the next save. Held here so it survives between frames
        // while the artist is typing it.
        static const int MAX_PRESET_NAME_INPUT = 32;
        char presetNameInput[MAX_PRESET_NAME_INPUT] = {};
    };
}

#endif // DEVUIVIEWPOSTPROCESS_H
