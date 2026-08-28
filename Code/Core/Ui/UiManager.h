#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include "UiCssParser.h"
#include "UiInputHandler.h"

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

        int lastScreenWidth = 0;
        int lastScreenHeight = 0;

        std::unordered_map<std::string, UiElement*> elementIdMap;
        std::vector<UiCssRule> activeCssRules;
        UiInputHandler inputHandler;

        void UnloadScreen();
        void BuildIdMap(UiElement* element);
        void RenderElement(UiElement* element);
        void RenderDropdownOptions(class UiDropdown* dropdown);

        class UiDropdown* pendingDropdown = nullptr;
    };
}

#endif // UIMANAGER_H
