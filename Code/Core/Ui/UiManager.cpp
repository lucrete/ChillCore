#include "UiManager.h"

#include "CCAssert.h"
#include "PlatformFileSystem.h"
#include "PrintManager.h"
#include "RenderManager.h"
#include "InputManager.h"
#include "UiScreenSystem.h"
#include "UiRenderer.h"
#include "UiElement.h"
#include "UiSlider.h"
#include "UiToggle.h"
#include "UiDropdown.h"
#include "UiCssParser.h"
#include "UiHtmlParser.h"
#include "UiLayoutEngine.h"
#include "TextRenderer.h"
#include "FontManager.h"
#include "Font.h"
#include "TextureManager.h"
#include "Texture.h"

namespace CC
{
    UiManager* UiManager::instance = nullptr;

    UiManager::UiManager()
    {
        CC_ASSERT(instance == nullptr, "UiManager already created");
        instance = this;
    }

    UiManager::~UiManager()
    {
        UnloadScreen();
        instance = nullptr;
    }

    UiManager* UiManager::Get()
    {
        CC_ASSERT(instance != nullptr, "UiManager not created yet");
        return instance;
    }

    // ========================
    // Screen loading
    // ========================

    UiElement* UiManager::BuildScreen(const std::string& htmlPath, const std::string& cssPath, std::vector<UiCssRule>& outCssRules)
    {
        UiElement* root = nullptr;
        outCssRules.clear();

        std::string cssContents;
        if (PlatformFileSystem::Get()->ReadFileText(cssPath.c_str(), cssContents))
        {
            outCssRules = UiCssParser::Parse(cssContents);

            root = UiHtmlParser::Parse(htmlPath, outCssRules);
            if (root)
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "UI screen built: %s", htmlPath.c_str());
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_WARN, "Failed to parse HTML file: %s", htmlPath.c_str());
            }
        }
        else
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to open CSS file: %s", cssPath.c_str());
        }

        return root;
    }

    void UiManager::LoadScreen(UiElement* root)
    {
        elementIdMap.clear();
        activeCssRules.clear();
        inputHandler.SetRootElement(nullptr);
        rootElement = nullptr;

        if (root)
        {
            rootElement = root;
            BuildIdMap(rootElement);
            inputHandler.SetRootElement(rootElement);
            isLayoutDirty = true;
        }
    }

    void UiManager::LoadScreen(UiElement* root, const std::vector<UiCssRule>& cssRules)
    {
        LoadScreen(root);
        activeCssRules = cssRules;
    }

    void UiManager::LoadScreen(const std::string& htmlPath, const std::string& cssPath)
    {
        UnloadScreen();

        rootElement = BuildScreen(htmlPath, cssPath, activeCssRules);
        if (rootElement)
        {
            BuildIdMap(rootElement);
            inputHandler.SetRootElement(rootElement);
            isLayoutDirty = true;
        }
    }

    void UiManager::ApplyStylesToDynamicSubtree(UiElement* element)
    {
        UiHtmlParser::ApplyStylesToSubtree(element, activeCssRules);
    }

    void UiManager::UnloadScreen()
    {
        elementIdMap.clear();
        inputHandler.SetRootElement(nullptr);
        inputHandler.SetCallbackMap(nullptr);

        if (rootElement)
        {
            delete rootElement;
            rootElement = nullptr;
        }
    }

    // ========================
    // Update + Render
    // ========================
    void UiManager::Update()
    {
        if (rootElement)
        {
            // Check for window resize
            int width, height;
            RenderManager::Get()->GetWindowSize(width, height);
            if (width != lastScreenWidth || height != lastScreenHeight)
            {
                lastScreenWidth = width;
                lastScreenHeight = height;
                isLayoutDirty = true;
            }

            // Run input handler whenever the HUD/UI can be interacted with.
            // The handler itself filters interactions by mode (UI vs HUD-only).
            if (InputManager::Get()->IsHudInteractable() && !UiScreenSystem::Get()->IsTransitioning())
            {
                inputHandler.Update();
            }
        }
    }

    void UiManager::Render()
    {
        if (rootElement)
        {
            int width, height;
            RenderManager::Get()->GetWindowSize(width, height);

            // Run layout if dirty
            if (isLayoutDirty)
            {
                UiLayoutEngine::ComputeLayout(rootElement, width, height);
                isLayoutDirty = false;
            }

            // Begin UI quad rendering
            UiRenderer::Get()->BeginFrame(width, height);

            // Walk tree depth-first, emit quads and text
            pendingDropdown = nullptr;
            RenderElement(rootElement);

            // Flush base layer (quads + text) before drawing dropdown overlay
            UiRenderer::Get()->EndFrame();
            TextRenderer::Get()->EndFrame();

            // Draw expanded dropdown overlay on top of everything
            if (pendingDropdown)
            {
                UiRenderer::Get()->BeginFrame(width, height);
                TextRenderer::Get()->BeginFrame(width, height);

                RenderDropdownOptions(pendingDropdown);
                pendingDropdown = nullptr;

                UiRenderer::Get()->EndFrame();
            }

            // TextRenderer::EndFrame() is called externally after this,
            // which draws the dropdown option text on top of dropdown quads.
        }
    }

    void UiManager::RenderElement(UiElement* element)
    {
        bool isVisible = element->IsVisible() && element->computedStyle.display != DisplayType::None;
        if (isVisible)
        {
            const UiRect& rect = element->layoutRect;
            const UiStyleProperties& style = element->computedStyle;

            // Draw background
            if (style.hasBackgroundColor && style.backgroundColor.a > 0.0f)
            {
                Colour bgColour = style.backgroundColor;
                bgColour.a *= style.opacity;
                UiRenderer::Get()->DrawQuad(rect.x, rect.y, rect.width, rect.height, bgColour);
            }

            // Draw background image
            if (style.hasBackgroundImage && !style.backgroundImage.empty())
            {
                Texture* bgTexture = TextureManager::Get()->GetTexture(style.backgroundImage);
                if (bgTexture)
                {
                    Colour tint = Colour::WHITE();
                    tint.a = style.opacity;

                    if (style.backgroundSlice > 0.0f)
                    {
                        float scaleFactor = (float)lastScreenHeight / 1080.0f;
                        float scaledSlice = style.backgroundSlice * scaleFactor;
                        UiRenderer::Get()->DrawSlice9(
                            rect.x, rect.y, rect.width, rect.height,
                            bgTexture->GetTextureHandle(), scaledSlice,
                            bgTexture->GetWidth(), bgTexture->GetHeight(), tint);
                    }
                    else
                    {
                        UiRenderer::Get()->DrawTexturedQuad(
                            rect.x, rect.y, rect.width, rect.height,
                            0.0f, 1.0f, 1.0f, 0.0f, tint, bgTexture->GetTextureHandle());
                    }
                }
            }

            // Draw border
            if (style.borderWidth > 0.0f && style.borderColor.a > 0.0f)
            {
                float scaledBorderWidth = style.borderWidth;
                Colour borderColour = style.borderColor;
                borderColour.a *= style.opacity;

                UiRenderer::Get()->DrawQuad(rect.x, rect.y, rect.width, scaledBorderWidth, borderColour);
                UiRenderer::Get()->DrawQuad(rect.x, rect.y + rect.height - scaledBorderWidth, rect.width, scaledBorderWidth, borderColour);
                UiRenderer::Get()->DrawQuad(rect.x, rect.y, scaledBorderWidth, rect.height, borderColour);
                UiRenderer::Get()->DrawQuad(rect.x + rect.width - scaledBorderWidth, rect.y, scaledBorderWidth, rect.height, borderColour);
            }

            // Draw text content (skip specialized elements that render their own)
            bool hasSpecializedRendering = element->GetType() == UiElementType::Slider
                || element->GetType() == UiElementType::Toggle
                || element->GetType() == UiElementType::Dropdown;
            if (!hasSpecializedRendering && !element->GetTextContent().empty() && style.hasFontSize)
            {
                Font* font = FontManager::Get()->GetFont(
                    style.hasFontFamily ? style.fontFamily : "robotoRegular");
                if (font)
                {
                    float screenHeight = (float)lastScreenHeight;
                    float scaleFactor = screenHeight / 1080.0f;
                    float fontSize = style.fontSize * scaleFactor;
                    float maxWidth = rect.width - (style.padding.left + style.padding.right) * scaleFactor;
                    float textX = rect.x + style.padding.left * scaleFactor;

                    // Vertically center text within the content area (cached to avoid per-frame MeasureText)
                    float contentHeight = rect.height - (style.padding.top + style.padding.bottom) * scaleFactor;
                    if (element->cachedTextHeight < 0.0f)
                    {
                        TextMetrics metrics = TextRenderer::Get()->MeasureText(
                            element->GetTextContent(), font, fontSize, maxWidth);
                        element->cachedTextHeight = metrics.height;
                    }
                    float textY = rect.y + style.padding.top * scaleFactor
                        + (contentHeight - element->cachedTextHeight) * 0.5f;

                    Colour textColour = style.color;
                    textColour.a *= style.opacity;

                    // Use cached vertex data when text and layout haven't changed
                    CachedTextDraw& cache = element->cachedTextDraw;
                    if (cache.Matches(element->GetTextContent(), font, fontSize,
                        textX, textY, maxWidth, style.textAlign))
                    {
                        TextRenderer::Get()->DrawCachedText(
                            cache.vertices, cache.indices, font, fontSize, textColour);
                    }
                    else
                    {
                        TextRenderer::Get()->LayoutTextToCache(
                            element->GetTextContent(), font, fontSize,
                            textX, textY, style.textAlign, maxWidth,
                            cache.vertices, cache.indices);

                        cache.text = element->GetTextContent();
                        cache.font = font;
                        cache.fontSize = fontSize;
                        cache.x = textX;
                        cache.y = textY;
                        cache.maxWidth = maxWidth;
                        cache.alignment = style.textAlign;
                        cache.isValid = true;

                        TextRenderer::Get()->DrawCachedText(
                            cache.vertices, cache.indices, font, fontSize, textColour);
                    }
                }
            }

            // ========================
            // Specialized element rendering
            // ========================

            float scaleFactor2 = (float)lastScreenHeight / 1080.0f;

            if (element->GetType() == UiElementType::Slider)
            {
                UiSlider* slider = static_cast<UiSlider*>(element);
                const UiStyleProperties& sliderNormal = element->stateStyles[static_cast<int>(UiElementState::Normal)];
                const UiStyleProperties& sliderHover = element->stateStyles[static_cast<int>(UiElementState::Hovered)];
                const UiStyleProperties& sliderActive = element->stateStyles[static_cast<int>(UiElementState::Pressed)];

                Colour trackColour = sliderNormal.hasColor ? sliderNormal.color : Colour(0.3f, 0.3f, 0.3f, 0.8f);
                Colour fillColour = sliderHover.hasColor ? sliderHover.color : Colour(0.4f, 0.6f, 1.0f, 1.0f);
                Colour thumbColour = sliderActive.hasColor ? sliderActive.color : Colour::WHITE();

                float trackHeight = 4.0f * scaleFactor2;
                float trackY = rect.y + (rect.height - trackHeight) * 0.5f;
                float trackPadding = style.padding.left * scaleFactor2;
                float trackWidth = rect.width - trackPadding * 2.0f;

                UiRenderer::Get()->DrawQuad(rect.x + trackPadding, trackY, trackWidth, trackHeight, trackColour);

                float fillWidth = trackWidth * slider->GetNormalizedValue();
                UiRenderer::Get()->DrawQuad(rect.x + trackPadding, trackY, fillWidth, trackHeight, fillColour);

                float thumbSize = 12.0f * scaleFactor2;
                float thumbX = rect.x + trackPadding + fillWidth - thumbSize * 0.5f;
                float thumbY = trackY + trackHeight * 0.5f - thumbSize * 0.5f;
                UiRenderer::Get()->DrawQuad(thumbX, thumbY, thumbSize, thumbSize, thumbColour);
            }
            else if (element->GetType() == UiElementType::Toggle)
            {
                UiToggle* toggle = static_cast<UiToggle*>(element);
                const UiStyleProperties& toggleNormal = element->stateStyles[static_cast<int>(UiElementState::Normal)];
                const UiStyleProperties& toggleHover = element->stateStyles[static_cast<int>(UiElementState::Hovered)];
                const UiStyleProperties& toggleActive = element->stateStyles[static_cast<int>(UiElementState::Pressed)];

                Colour uncheckedColour = toggleNormal.hasColor ? toggleNormal.color : Colour(0.4f, 0.4f, 0.4f, 1.0f);
                Colour checkedColour = toggleHover.hasColor ? toggleHover.color : Colour(0.3f, 0.7f, 0.3f, 1.0f);
                Colour indicatorColour = toggleActive.hasColor ? toggleActive.color : Colour::WHITE();

                float toggleWidth = 40.0f * scaleFactor2;
                float toggleHeight = 20.0f * scaleFactor2;
                float toggleX = rect.x + style.padding.left * scaleFactor2;
                float toggleY = rect.y + (rect.height - toggleHeight) * 0.5f;

                Colour trackColour = toggle->IsChecked() ? checkedColour : uncheckedColour;
                UiRenderer::Get()->DrawQuad(toggleX, toggleY, toggleWidth, toggleHeight, trackColour);

                float indicatorSize = toggleHeight - 4.0f * scaleFactor2;
                float indicatorPad = 2.0f * scaleFactor2;
                float indicatorX = toggle->IsChecked() ?
                    toggleX + toggleWidth - indicatorSize - indicatorPad : toggleX + indicatorPad;
                float indicatorY = toggleY + indicatorPad;
                UiRenderer::Get()->DrawQuad(indicatorX, indicatorY, indicatorSize, indicatorSize, indicatorColour);
            }
            else if (element->GetType() == UiElementType::Dropdown)
            {
                UiDropdown* dropdown = static_cast<UiDropdown*>(element);
                Font* font = FontManager::Get()->GetFont(
                    style.hasFontFamily ? style.fontFamily : "robotoRegular");
                float fontSize = style.hasFontSize ? style.fontSize * scaleFactor2 : 16.0f * scaleFactor2;

                if (font && !dropdown->GetSelectedText().empty())
                {
                    float textX = rect.x + style.padding.left * scaleFactor2;
                    float textY = rect.y + style.padding.top * scaleFactor2;
                    TextRenderer::Get()->DrawText(dropdown->GetSelectedText(), font, fontSize,
                        textX, textY, style.color, TextAlign::Left);
                }

                if (dropdown->IsExpanded())
                {
                    pendingDropdown = dropdown;
                }
            }

            // Recurse into children
            for (UiElement* child : element->GetChildren())
            {
                RenderElement(child);
            }
        }
    }

    void UiManager::RenderDropdownOptions(UiDropdown* dropdown)
    {
        float scaleFactor = (float)lastScreenHeight / 1080.0f;
        const UiRect& rect = dropdown->layoutRect;
        const UiStyleProperties& normalStyle = dropdown->stateStyles[static_cast<int>(UiElementState::Normal)];
        const UiStyleProperties& hoverStyle = dropdown->stateStyles[static_cast<int>(UiElementState::Hovered)];
        const UiStyleProperties& activeStyle = dropdown->stateStyles[static_cast<int>(UiElementState::Pressed)];

        Font* font = FontManager::Get()->GetFont(
            normalStyle.hasFontFamily ? normalStyle.fontFamily : "robotoRegular");
        if (font)
        {
            float fontSize = normalStyle.hasFontSize ? normalStyle.fontSize * scaleFactor : 16.0f * scaleFactor;
            float optionY = rect.y + rect.height;
            float optionHeight = fontSize * 1.5f;
            const std::vector<std::string>& options = dropdown->GetOptions();

            Colour normalBg = normalStyle.hasBackgroundColor ? normalStyle.backgroundColor : Colour(0.15f, 0.15f, 0.25f, 1.0f);
            Colour hoverBg = hoverStyle.hasBackgroundColor ? hoverStyle.backgroundColor : normalBg;
            Colour activeBg = activeStyle.hasBackgroundColor ? activeStyle.backgroundColor : normalBg;
            Colour normalTextColour = normalStyle.hasColor ? normalStyle.color : Colour::WHITE();
            Colour hoverTextColour = hoverStyle.hasColor ? hoverStyle.color : normalTextColour;
            Colour activeTextColour = activeStyle.hasColor ? activeStyle.color : normalTextColour;

            int hoveredOption = dropdown->GetHoveredOption();
            int selectedOption = dropdown->GetSelectedOption();

            for (int i = 0; i < (int)options.size(); i++)
            {
                Colour optionBg;
                Colour optionText;
                if (i == selectedOption)
                {
                    optionBg = activeBg;
                    optionText = activeTextColour;
                }
                else if (i == hoveredOption)
                {
                    optionBg = hoverBg;
                    optionText = hoverTextColour;
                }
                else
                {
                    optionBg = normalBg;
                    optionText = normalTextColour;
                }
                UiRenderer::Get()->DrawQuad(rect.x, optionY, rect.width, optionHeight, optionBg);

                float textX = rect.x + normalStyle.padding.left * scaleFactor;
                float textY = optionY + (optionHeight - fontSize) * 0.25f;
                TextRenderer::Get()->DrawText(options[i], font, fontSize,
                    textX, textY, optionText, TextAlign::Left);

                optionY += optionHeight;
            }
        }
    }

    // ========================
    // Element lookup
    // ========================

    UiElement* UiManager::GetElementById(const std::string& id) const
    {
        UiElement* result = nullptr;
        auto it = elementIdMap.find(id);
        if (it != elementIdMap.end())
        {
            result = it->second;
        }
        return result;
    }

    void UiManager::BuildIdMap(UiElement* element)
    {
        if (!element->GetId().empty())
        {
            elementIdMap[element->GetId()] = element;
        }
        for (UiElement* child : element->GetChildren())
        {
            BuildIdMap(child);
        }
    }

    // ========================
    // Callback registration
    // ========================

    void UiManager::RegisterButtonAction(const std::string& key, std::function<void()> callback)
    {
        UiCallbackMap* map = inputHandler.GetCallbackMap();
        CC_ASSERT(map != nullptr, "No active callback map");
        map->RegisterButton(key, callback);
    }

    void UiManager::RegisterSliderAction(const std::string& key, std::function<void(float)> callback)
    {
        UiCallbackMap* map = inputHandler.GetCallbackMap();
        CC_ASSERT(map != nullptr, "No active callback map");
        map->RegisterSlider(key, callback);
    }

    void UiManager::RegisterToggleAction(const std::string& key, std::function<void(bool)> callback)
    {
        UiCallbackMap* map = inputHandler.GetCallbackMap();
        CC_ASSERT(map != nullptr, "No active callback map");
        map->RegisterToggle(key, callback);
    }

    void UiManager::RegisterDropdownAction(const std::string& key, std::function<void(int)> callback)
    {
        UiCallbackMap* map = inputHandler.GetCallbackMap();
        CC_ASSERT(map != nullptr, "No active callback map");
        map->RegisterDropdown(key, callback);
    }

    void UiManager::RegisterJoystickAction(const std::string& key, std::function<void(float, float)> callback)
    {
        UiCallbackMap* map = inputHandler.GetCallbackMap();
        CC_ASSERT(map != nullptr, "No active callback map");
        map->RegisterJoystick(key, callback);
    }

}
