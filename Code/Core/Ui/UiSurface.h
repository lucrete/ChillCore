#ifndef UISURFACE_H
#define UISURFACE_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "GfxHandles.h"
#include "UiCssParser.h"
#include "UiInputHandler.h"
#include "UiScreenSystem.h"

namespace CC
{
    class UiDropdown;
    class UiElement;

    // What a surface draws into. Whoever creates a surface states intent and
    // a size; the surface allocates and formats whatever that needs.
    enum class UiSurfaceKind
    {
        // Draws straight into whatever the frame has bound, at the window's
        // size. The engine owns the one of these that exists.
        Window,

        // Owns an offscreen target and hands out the texture it drew into.
        Offscreen,

        // Real, with nothing behind it. Every call succeeds and draws
        // nothing, so application code never branches on the target.
        Null
    };

    // One root element, one id map, one css rule set, one layout state, one
    // size, one screen stack and its pointers. Everything that used to be
    // the single global UI.
    class UiSurface
    {
    public:
        UiSurface(const std::string& name, UiSurfaceKind kind, int width, int height);
        ~UiSurface();

        const std::string& GetName() const { return name; }
        UiSurfaceKind      GetKind() const { return kind; }

        // A disabled surface skips its update, hit test and render, and
        // keeps its target allocated.
        void SetEnabled(bool _isEnabled) { isEnabled = _isEnabled; }
        bool IsEnabled() const { return isEnabled; }

        // ========================
        // Screens
        // ========================

        UiScreenSystem& Screens() { return screens; }

        // ========================
        // Element tree
        // ========================

        void LoadScreen(UiElement* root);
        void LoadScreen(UiElement* root, const std::vector<UiCssRule>& cssRules);
        void LoadScreen(const std::string& htmlPath, const std::string& cssPath);

        // Applies the active screen's CSS rules to a subtree of newly built
        // elements. Required when controllers add elements at runtime —
        // UiHtmlParser only applies rules at parse time, so dynamic subtrees
        // would otherwise render with empty styles.
        void ApplyStylesToDynamicSubtree(UiElement* element);

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

        // ========================
        // Size
        // ========================

        // The one place anything scaling against this surface reads its size
        // from. Layout, text sizing and hit testing disagree the moment two
        // of them ask different sources.
        void GetSize(int& outWidth, int& outHeight) const;
        void SetSize(int newWidth, int newHeight);

        // The result of an offscreen surface, rather than the target it drew
        // into. Invalid on a window or null surface.
        Gfx::TextureHandle GetOutputTexture() const { return colourTexture; }

        // ========================
        // Redraw
        // ========================

        // Force the next render to re-run UiLayoutEngine. Layout otherwise
        // only re-runs on resize and screen load.
        void InvalidateLayout();

        // The surface has something new to draw. Marked automatically by the
        // element setters and by hover and press transitions; a missed mark
        // shows as a panel frozen on stale content.
        void MarkContentDirty() { isContentDirty = true; }
        bool IsContentDirty() const { return isContentDirty; }

        // ========================
        // Per-frame
        // ========================

        void Update();

        // Draws into whatever is bound. Window surfaces only.
        void RenderDirect();

        // Draws into this surface's own target, and only when something
        // changed. A static panel nobody is pointing at costs nothing.
        void RenderOffscreen();

    private:
        std::string   name;
        UiSurfaceKind kind;

        int  width  = 0;
        int  height = 0;
        bool isEnabled = true;

        UiElement*                                 rootElement = nullptr;
        std::unordered_map<std::string, UiElement*> elementIdMap;
        std::vector<UiCssRule>                      activeCssRules;

        bool isLayoutDirty  = true;
        bool isContentDirty = true;

        UiInputHandler inputHandler;
        UiScreenSystem screens;

        Gfx::TextureHandle          colourTexture;
        Gfx::RenderTargetHandle     renderTarget;

        UiDropdown* pendingDropdown = nullptr;

        void CreateTarget();
        void DestroyTarget();

        void RenderScreen();
        void RenderElement(UiElement* element);
        void RenderDropdownOptions(UiDropdown* dropdown);

        void BindTree(UiElement* element);
    };
}

#endif // UISURFACE_H
