#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <string>
#include <vector>

#include "UiCssParser.h"
#include "UiPointerRouter.h"

namespace CC
{
    class UiElement;
    class UiSurface;

    // Owns every UI surface and creates them on request. The engine owns the
    // window's surface, because the UI is an engine feature rather than
    // something an application assembles.
    class UiManager
    {
    public:
        UiManager();
        ~UiManager();
        static UiManager* Get();

        // Parses markup and styles into an element tree. Belongs to no
        // surface: the tree is bound to one when it is loaded.
        static UiElement* BuildScreen(const std::string& htmlPath, const std::string& cssPath,
                                      std::vector<UiCssRule>& outCssRules);

        // ========================
        // Surfaces
        // ========================
        // The window's surface. Where no window exists this is real and has
        // nothing behind it, so application code never branches on it.
        UiSurface* GetWindowSurface() const { return windowSurface; }

        // An offscreen surface of this size in pixels. The caller states a
        // size and gets back a surface whose output texture is the result;
        // the target behind it is the surface's own business.
        UiSurface* CreateSurface(const std::string& name, int width, int height);
        void DestroySurface(UiSurface* surface);

        // Resolves world pointers to whichever hit-testable target is nearest.
        UiPointerRouter& GetPointerRouter() { return pointerRouter; }

        // ========================
        // Per-frame
        // ========================
        // Screen stacks and their transitions, every surface.
        void UpdateScreens();

        // Layout resize checks and pointer input, every surface.
        void Update();

        // Offscreen surfaces, drawn before the scene that samples them.
        void RenderSurfaces();

        // The window surface, drawn into whatever the frame has bound.
        void Render();

    private:
        static UiManager* instance;

        UiSurface*              windowSurface = nullptr;
        std::vector<UiSurface*> surfaces;
        UiPointerRouter         pointerRouter;
    };
}

#endif // UIMANAGER_H
