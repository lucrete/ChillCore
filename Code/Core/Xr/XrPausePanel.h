#ifndef XRPAUSEPANEL_H
#define XRPAUSEPANEL_H

#include "Component.h"

namespace CC
{
    class UiSurface;
    class UiWorldPanel;

    // Shows and hides a world panel as the in-headset pause menu. Showing it
    // places it once, in front of the head: upright, facing the player, at
    // head height. It does not follow the head afterwards.
    //
    // Needs a UiWorldPanel on the same owner, and is hidden until shown.
    class XrPausePanel : public Component
    {
    public:
        XrPausePanel();
        virtual ~XrPausePanel();

        virtual const char* GetTypeName() const override { return "XrPausePanel"; }

        virtual void Init() override;

        void Show();
        void Hide();

        // Where the pause screen is registered. Valid from Init onwards.
        UiSurface* GetSurface() const;

    private:
        static constexpr float DISTANCE_FROM_HEAD_METRES = 1.0f;

        UiWorldPanel* panel = nullptr;

        void PlaceInFrontOfHead();
    };
}

#endif // XRPAUSEPANEL_H
