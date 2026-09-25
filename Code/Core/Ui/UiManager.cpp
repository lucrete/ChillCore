#include "UiManager.h"

#include "CCAssert.h"
#include "GfxRenderApi.h"
#include "InputManager.h"
#include "PlatformFileSystem.h"
#include "PlatformWindow.h"
#include "PrintManager.h"
#include "RenderManager.h"
#include "UiCssParser.h"
#include "UiElement.h"
#include "UiHtmlParser.h"
#include "UiSurface.h"

namespace CC
{
    UiManager* UiManager::instance = nullptr;

    UiManager::UiManager()
    {
        CC_ASSERT(instance == nullptr, "UiManager already created");
        instance = this;

        // A standalone headset has no window to draw to, and gets a surface
        // that is real and has nothing behind it rather than none at all.
        UiSurfaceKind windowKind = PlatformWindow::Get() != nullptr
            ? UiSurfaceKind::Window
            : UiSurfaceKind::Null;

        int width  = 0;
        int height = 0;
        if (windowKind == UiSurfaceKind::Window)
        {
            PlatformWindow::Get()->GetFramebufferSize(width, height);
        }

        windowSurface = new UiSurface("Window", windowKind, width, height);
        surfaces.push_back(windowSurface);

        InputManager::Get()->SetWindowSurface(windowSurface);
    }

    UiManager::~UiManager()
    {
        for (UiSurface* surface : surfaces)
        {
            delete surface;
        }
        surfaces.clear();
        windowSurface = nullptr;
        InputManager::Get()->SetWindowSurface(nullptr);

        instance = nullptr;
    }

    UiManager* UiManager::Get()
    {
        CC_ASSERT(instance != nullptr, "UiManager not created yet");
        return instance;
    }

    // ========================
    // Screen building
    // ========================
    UiElement* UiManager::BuildScreen(const std::string& htmlPath, const std::string& cssPath,
                                      std::vector<UiCssRule>& outCssRules)
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

    // ========================
    // Surfaces
    // ========================
    UiSurface* UiManager::CreateSurface(const std::string& name, int width, int height)
    {
        UiSurface* surface = new UiSurface(name, UiSurfaceKind::Offscreen, width, height);
        surfaces.push_back(surface);

        CCPrint(PrintManager::CHANNEL_ALWAYS, "UI surface created: %s (%dx%d)",
                name.c_str(), width, height);

        return surface;
    }

    void UiManager::DestroySurface(UiSurface* surface)
    {
        CC_ASSERT(surface != windowSurface, "The window surface is the engine's");

        for (size_t i = 0; i < surfaces.size(); i++)
        {
            if (surfaces[i] == surface)
            {
                surfaces.erase(surfaces.begin() + i);
                delete surface;
                break;
            }
        }
    }

    // ========================
    // Per-frame
    // ========================
    void UiManager::UpdateScreens()
    {
        for (UiSurface* surface : surfaces)
        {
            if (surface->IsEnabled())
            {
                surface->Screens().Update();
            }
        }
    }

    void UiManager::Update()
    {
        // Pointers that submitted nothing this frame release whatever they
        // were on, before any surface reads its pointer state.
        pointerRouter.Resolve();

        for (UiSurface* surface : surfaces)
        {
            surface->Update();
        }
    }

    void UiManager::RenderSurfaces()
    {
        for (UiSurface* surface : surfaces)
        {
            if (surface != windowSurface)
            {
                surface->RenderOffscreen();
            }
        }
    }

    void UiManager::Render()
    {
        windowSurface->RenderDirect();
    }
}
