#include "UiSurface.h"

#include "CCAssert.h"
#include "Font.h"
#include "FontManager.h"
#include "GfxRenderApi.h"
#include "InputManager.h"
#include "PrintManager.h"
#include "RenderManager.h"
#include "TextRenderer.h"
#include "Texture.h"
#include "TextureManager.h"
#include "UiCssParser.h"
#include "UiDropdown.h"
#include "UiElement.h"
#include "UiHtmlParser.h"
#include "UiLayoutEngine.h"
#include "UiManager.h"
#include "UiRenderer.h"
#include "UiSlider.h"
#include "UiToggle.h"

namespace CC
{
    // The size UI dimensions are authored against. Everything scales from it.
    static constexpr float REFERENCE_SURFACE_HEIGHT = 1080.0f;

    UiSurface::UiSurface(const std::string& _name, UiSurfaceKind _kind, int _width, int _height)
        : name(_name)
        , kind(_kind)
        , width(_width)
        , height(_height)
        , screens(this)
    {
        inputHandler.SetSurface(this);
        inputHandler.SetPointerSource(kind == UiSurfaceKind::Window
            ? UiPointerSource::Platform
            : UiPointerSource::External);

        if (kind == UiSurfaceKind::Offscreen)
        {
            CreateTarget();
        }
    }

    UiSurface::~UiSurface()
    {
        screens.ClearAllScreens();

        elementIdMap.clear();
        inputHandler.SetRootElement(nullptr);
        inputHandler.SetCallbackMap(nullptr);
        rootElement = nullptr;

        DestroyTarget();
    }

    // ========================
    // Target
    // ========================
    void UiSurface::CreateTarget()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        Gfx::TextureDescription colourDesc;
        colourDesc.width          = width;
        colourDesc.height         = height;
        colourDesc.format         = Gfx::TextureFormat::Rgba8Unorm;
        colourDesc.isRenderTarget = true;
        colourDesc.debugName      = name.c_str();
        colourTexture = gfxApi->CreateTexture(colourDesc);

        Gfx::RenderTargetDescription targetDesc;
        targetDesc.width                       = width;
        targetDesc.height                      = height;
        targetDesc.colorAttachmentCount        = 1;
        targetDesc.colorAttachments[0].texture = colourTexture;
        targetDesc.colorAttachments[0].loadOp  = Gfx::LoadOp::Clear;
        targetDesc.colorAttachments[0].storeOp = Gfx::StoreOp::Store;
        // Transparent, not the attachment default of opaque black: whatever
        // the screen does not cover has to disappear rather than become a slab.
        targetDesc.colorAttachments[0].clearColor[0] = 0.0f;
        targetDesc.colorAttachments[0].clearColor[1] = 0.0f;
        targetDesc.colorAttachments[0].clearColor[2] = 0.0f;
        targetDesc.colorAttachments[0].clearColor[3] = 0.0f;
        targetDesc.hasDepthStencil             = false;
        targetDesc.sampleCount                 = 1;
        targetDesc.debugName                   = name.c_str();
        renderTarget = gfxApi->CreateRenderTarget(targetDesc);

        isContentDirty = true;
    }

    void UiSurface::DestroyTarget()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        if (renderTarget.IsValid())
        {
            gfxApi->DestroyRenderTarget(renderTarget);
            renderTarget = Gfx::RenderTargetHandle();
        }
        if (colourTexture.IsValid())
        {
            gfxApi->DestroyTexture(colourTexture);
            colourTexture = Gfx::TextureHandle();
        }
    }

    // ========================
    // Element tree
    // ========================
    void UiSurface::LoadScreen(UiElement* root)
    {
        elementIdMap.clear();
        activeCssRules.clear();
        inputHandler.SetRootElement(nullptr);
        rootElement = nullptr;

        if (root)
        {
            rootElement = root;
            BindTree(rootElement);
            inputHandler.SetRootElement(rootElement);
        }

        isLayoutDirty  = true;
        isContentDirty = true;
    }

    void UiSurface::LoadScreen(UiElement* root, const std::vector<UiCssRule>& cssRules)
    {
        LoadScreen(root);
        activeCssRules = cssRules;
    }

    void UiSurface::LoadScreen(const std::string& htmlPath, const std::string& cssPath)
    {
        std::vector<UiCssRule> cssRules;
        UiElement* root = UiManager::BuildScreen(htmlPath, cssPath, cssRules);
        LoadScreen(root, cssRules);
    }

    void UiSurface::ApplyStylesToDynamicSubtree(UiElement* element)
    {
        UiHtmlParser::ApplyStylesToSubtree(element, activeCssRules);
        BindTree(element);
        MarkContentDirty();
    }

    // Every element carries the surface it belongs to, which is how a setter
    // deep in a controller marks the right panel for redraw without anything
    // being told which surface is current.
    void UiSurface::BindTree(UiElement* element)
    {
        element->SetSurface(this);

        if (!element->GetId().empty())
        {
            elementIdMap[element->GetId()] = element;
        }

        for (UiElement* child : element->GetChildren())
        {
            BindTree(child);
        }
    }

    UiElement* UiSurface::GetElementById(const std::string& id) const
    {
        UiElement* result = nullptr;

        auto it = elementIdMap.find(id);
        if (it != elementIdMap.end())
        {
            result = it->second;
        }

        return result;
    }

    // ========================
    // Callback registration
    // ========================
    void UiSurface::RegisterButtonAction(const std::string& key, std::function<void()> callback)
    {
        UiCallbackMap* map = inputHandler.GetCallbackMap();
        CC_ASSERT(map != nullptr, "No active callback map");
        map->RegisterButton(key, callback);
    }

    void UiSurface::RegisterSliderAction(const std::string& key, std::function<void(float)> callback)
    {
        UiCallbackMap* map = inputHandler.GetCallbackMap();
        CC_ASSERT(map != nullptr, "No active callback map");
        map->RegisterSlider(key, callback);
    }

    void UiSurface::RegisterToggleAction(const std::string& key, std::function<void(bool)> callback)
    {
        UiCallbackMap* map = inputHandler.GetCallbackMap();
        CC_ASSERT(map != nullptr, "No active callback map");
        map->RegisterToggle(key, callback);
    }

    void UiSurface::RegisterDropdownAction(const std::string& key, std::function<void(int)> callback)
    {
        UiCallbackMap* map = inputHandler.GetCallbackMap();
        CC_ASSERT(map != nullptr, "No active callback map");
        map->RegisterDropdown(key, callback);
    }

    void UiSurface::RegisterJoystickAction(const std::string& key, std::function<void(float, float)> callback)
    {
        UiCallbackMap* map = inputHandler.GetCallbackMap();
        CC_ASSERT(map != nullptr, "No active callback map");
        map->RegisterJoystick(key, callback);
    }

    // ========================
    // Size
    // ========================
    void UiSurface::GetSize(int& outWidth, int& outHeight) const
    {
        outWidth  = width;
        outHeight = height;
    }

    void UiSurface::SetSize(int newWidth, int newHeight)
    {
        if (newWidth != width || newHeight != height)
        {
            width  = newWidth;
            height = newHeight;

            if (kind == UiSurfaceKind::Offscreen)
            {
                DestroyTarget();
                CreateTarget();
            }

            InvalidateLayout();
        }
    }

    void UiSurface::InvalidateLayout()
    {
        isLayoutDirty  = true;
        isContentDirty = true;
    }

    // ========================
    // Per-frame
    // ========================
    void UiSurface::Update()
    {
        if (kind != UiSurfaceKind::Null && isEnabled)
        {
            if (kind == UiSurfaceKind::Window)
            {
                int windowWidth  = 0;
                int windowHeight = 0;
                RenderManager::Get()->GetWindowSize(windowWidth, windowHeight);
                SetSize(windowWidth, windowHeight);
            }

            // The window takes input only while it is the Window domain's
            // target. A panel has only the pointers the router put on it,
            // and the router already offers them by the Headset target.
            bool isInteractable = kind != UiSurfaceKind::Window
                || InputManager::Get()->DoesSurfaceReceiveInput(InputDomain::Window, this);

            if (rootElement != nullptr && isInteractable && !screens.IsTransitioning())
            {
                inputHandler.Update();
            }
        }
    }

    void UiSurface::RenderDirect()
    {
        if (kind == UiSurfaceKind::Window && rootElement != nullptr)
        {
            float fade = screens.GetFadeAlpha();
            UiRenderer::Get()->SetGlobalAlpha(fade);
            TextRenderer::Get()->SetGlobalAlpha(fade);

            RenderScreen();
        }
    }

    void UiSurface::RenderOffscreen()
    {
        // A transition is a fade, which changes every frame it runs.
        if (isEnabled && screens.IsTransitioning())
        {
            MarkContentDirty();
        }

        // Runs with no screen loaded too. The pass is what clears the target
        // to transparent, and a panel that has never been cleared shows
        // whatever the texture was allocated over.
        if (kind == UiSurfaceKind::Offscreen && isEnabled
            && renderTarget.IsValid() && isContentDirty)
        {
            Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

            // A clean surface emits no scope at all, so the detail names
            // under this phase vary between frames.
            std::string scopeName = "UI/" + name;
            gfxApi->AddGpuTimestamp(scopeName.c_str());

            gfxApi->BeginRenderPass(renderTarget, scopeName.c_str());
            gfxApi->InvalidateCachedState();

            float fade = screens.GetFadeAlpha();
            UiRenderer::Get()->SetGlobalAlpha(fade);
            TextRenderer::Get()->SetGlobalAlpha(fade);

            // Text carries its own projection, set for the window by the
            // frame start. This surface needs its own, and the window's has
            // to come back before anything else in the frame draws text.
            // RenderScreen flushes the text batch itself; flushing again
            // here would redraw every batch, because EndFrame draws what is
            // queued and only BeginFrame clears it.
            TextRenderer::Get()->BeginFrame(width, height);
            if (rootElement != nullptr)
            {
                RenderScreen();
            }

            gfxApi->EndRenderPass();

            int windowWidth  = 0;
            int windowHeight = 0;
            RenderManager::Get()->GetWindowSize(windowWidth, windowHeight);
            TextRenderer::Get()->BeginFrame(windowWidth, windowHeight);

            isContentDirty = false;
        }
    }

    void UiSurface::RenderScreen()
    {
        if (isLayoutDirty)
        {
            UiLayoutEngine::ComputeLayout(rootElement, width, height);
            isLayoutDirty = false;
        }

        UiRenderer::Get()->BeginFrame(width, height);

        pendingDropdown = nullptr;
        RenderElement(rootElement);

        // Flush the base layer before drawing the dropdown overlay on top.
        UiRenderer::Get()->EndFrame();
        TextRenderer::Get()->EndFrame();

        if (pendingDropdown)
        {
            UiRenderer::Get()->BeginFrame(width, height);
            TextRenderer::Get()->BeginFrame(width, height);

            RenderDropdownOptions(pendingDropdown);
            pendingDropdown = nullptr;

            UiRenderer::Get()->EndFrame();
        }

        // TextRenderer::EndFrame() is called by the caller after this, which
        // draws the dropdown option text on top of the dropdown quads.
    }

    void UiSurface::RenderElement(UiElement* element)
    {
        bool isVisible = element->IsVisible() && element->computedStyle.display != DisplayType::None;
        if (isVisible)
        {
            const UiRect& rect = element->layoutRect;
            const UiStyleProperties& style = element->computedStyle;
            const float scaleFactor = (float)height / REFERENCE_SURFACE_HEIGHT;

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
            if (element->GetType() == UiElementType::Slider)
            {
                UiSlider* slider = static_cast<UiSlider*>(element);
                const UiStyleProperties& sliderNormal = element->stateStyles[static_cast<int>(UiElementState::Normal)];
                const UiStyleProperties& sliderHover = element->stateStyles[static_cast<int>(UiElementState::Hovered)];
                const UiStyleProperties& sliderActive = element->stateStyles[static_cast<int>(UiElementState::Pressed)];

                Colour trackColour = sliderNormal.hasColor ? sliderNormal.color : Colour(0.3f, 0.3f, 0.3f, 0.8f);
                Colour fillColour = sliderHover.hasColor ? sliderHover.color : Colour(0.4f, 0.6f, 1.0f, 1.0f);
                Colour thumbColour = sliderActive.hasColor ? sliderActive.color : Colour::WHITE();

                float trackHeight = 4.0f * scaleFactor;
                float trackY = rect.y + (rect.height - trackHeight) * 0.5f;
                float trackPadding = style.padding.left * scaleFactor;
                float trackWidth = rect.width - trackPadding * 2.0f;

                UiRenderer::Get()->DrawQuad(rect.x + trackPadding, trackY, trackWidth, trackHeight, trackColour);

                float fillWidth = trackWidth * slider->GetNormalizedValue();
                UiRenderer::Get()->DrawQuad(rect.x + trackPadding, trackY, fillWidth, trackHeight, fillColour);

                float thumbSize = 12.0f * scaleFactor;
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

                float toggleWidth = 40.0f * scaleFactor;
                float toggleHeight = 20.0f * scaleFactor;
                float toggleX = rect.x + style.padding.left * scaleFactor;
                float toggleY = rect.y + (rect.height - toggleHeight) * 0.5f;

                Colour trackColour = toggle->IsChecked() ? checkedColour : uncheckedColour;
                UiRenderer::Get()->DrawQuad(toggleX, toggleY, toggleWidth, toggleHeight, trackColour);

                float indicatorSize = toggleHeight - 4.0f * scaleFactor;
                float indicatorPad = 2.0f * scaleFactor;
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
                float fontSize = style.hasFontSize ? style.fontSize * scaleFactor : 16.0f * scaleFactor;

                if (font && !dropdown->GetSelectedText().empty())
                {
                    float textX = rect.x + style.padding.left * scaleFactor;
                    float textY = rect.y + style.padding.top * scaleFactor;
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

    void UiSurface::RenderDropdownOptions(UiDropdown* dropdown)
    {
        float scaleFactor = (float)height / REFERENCE_SURFACE_HEIGHT;
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
}
