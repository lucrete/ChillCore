#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include "UiCssParser.h"
#include "UiInputHandler.h"
#include "GfxHandles.h"

namespace CC
{
    class UiElement;

    class UiManager
    {
    public:
        UiManager();
        ~UiManager();
        static UiManager* Get();

        UiElement* BuildScreen(const std::string& htmlPath, const std::string& cssPath, std::vector<UiCssRule>& outCssRules);
        void LoadScreen(UiElement* root);
        void LoadScreen(UiElement* root, const std::vector<UiCssRule>& cssRules);
        void LoadScreen(const std::string& htmlPath, const std::string& cssPath);

        // Applies the active screen's CSS rules to a subtree of newly
        // built elements. Required when controllers add elements at
        // runtime — UiHtmlParser only applies rules at parse time, so
        // dynamic subtrees would otherwise render with empty styles.
        void ApplyStylesToDynamicSubtree(UiElement* element);

        void Update();
        void Render();

        // Moves the screen off the window and onto a panel of this size.
        // Layout, hit testing and drawing then all happen against the panel,
        // and the window pass draws nothing — a pointer supplied in panel
        // pixels has to meet element rects laid out in the same space, and
        // laying out twice per frame at two sizes leaves them disagreeing.
        void SetPanelSurface(int width, int height);
        void ClearPanelSurface();
        bool HasPanelSurface() const { return hasPanelSurface; }

        // The surface the screen is currently laid out and drawn against —
        // the panel where one is set, the window otherwise. Everything that
        // scales against the surface must read it from here: layout, text
        // sizing and hit testing disagree the moment two of them ask
        // different sources.
        void GetSurfaceSize(int& outWidth, int& outHeight) const;

        // Draws the active screen into an offscreen target at the panel size,
        // so it can be sampled as a texture. Does nothing until a panel
        // surface is set.
        void RenderToTarget(Gfx::RenderTargetHandle target);

        UiElement* GetElementById(const std::string& id) const;

        // ========================
        // Callback registration
        // ========================

        void RegisterButtonAction(const std::string& key, std::function<void()> callback);
        void RegisterSliderAction(const std::string& key, std::function<void(float)> callback);
        void RegisterToggleAction(const std::string& key, std::function<void(bool)> callback);
        void RegisterDropdownAction(const std::string& key, std::function<void(int)> callback);
        void RegisterJoystickAction(const std::string& key, std::function<void(float, float)> callback);

        UiInputHandler& GetInputHandler() { return inputHandler; }

        // Force the next Render pass to re-run UiLayoutEngine. Use after
        // mutations that affect element rects (e.g. SetVisible toggling)
        // since layout otherwise only re-runs on screen resize.
        void InvalidateLayout() { isLayoutDirty = true; }

    private:
        static UiManager* instance;

        UiElement* rootElement = nullptr;
        bool isLayoutDirty = true;

        int  panelSurfaceWidth = 0;
        int  panelSurfaceHeight = 0;
        bool hasPanelSurface = false;

        int lastScreenWidth = 0;
        int lastScreenHeight = 0;

        std::unordered_map<std::string, UiElement*> elementIdMap;
        std::vector<UiCssRule> activeCssRules;
        UiInputHandler inputHandler;

        // The body both render paths share, given the surface to lay out
        // and draw against.
        void RenderScreen(int width, int height);

        void UnloadScreen();
        void BuildIdMap(UiElement* element);
        void RenderElement(UiElement* element);
        void RenderDropdownOptions(class UiDropdown* dropdown);

        class UiDropdown* pendingDropdown = nullptr;
    };
}

#endif // UIMANAGER_H
