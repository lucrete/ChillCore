#ifndef UISCREENCONTROLLER_H
#define UISCREENCONTROLLER_H

namespace CC
{
    class UiSurface;

    class UiScreenController
    {
    public:
        virtual ~UiScreenController() = default;

        // Bound at RegisterScreen and never changed. A controller resolves
        // its elements and callbacks against this rather than against an
        // implicit current surface, so the same controller class works on
        // the window and on a panel with no change.
        void SetSurface(UiSurface* _surface) { surface = _surface; }
        UiSurface* GetSurface() const { return surface; }

        // Called once at RegisterScreen. Element tree is built, elements exist.
        // Register callbacks, set text from StringDb, any one-time setup.
        virtual void Init() {}

        // Called each time screen becomes active. Per-activation state only:
        // enable/disable menu items, set nav direction, etc.
        virtual void OnEnter() {}

        // Called each time screen is deactivated.
        virtual void OnExit() {}

        // Per-frame update while screen is active and not transitioning.
        virtual void OnUpdate() {}

        // Localization refresh. Called from Init() and on language change.
        virtual void UpdateStrings() {}

    protected:
        UiSurface* surface = nullptr;
    };
}

#endif // UISCREENCONTROLLER_H
