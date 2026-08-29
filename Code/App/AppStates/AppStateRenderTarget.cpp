#include "AppStateRenderTarget.h"

#include "CameraManager.h"
#include "CoreMain.h"
#include "GfxRenderApi.h"
#include "GfxDescriptions.h"
#include "InputManager.h"
#include "MaterialManager.h"
#include "Material.h"
#include "PrintManager.h"
#include "RenderableSphere.h"
#include "RenderManager.h"
#include "RotateRandom.h"
#include "SceneHierarchy.h"
#include "SceneObject.h"
#include "StateMachine.h"

namespace
{
    const char* POST_MATERIAL_NAME = "PostProcessInvert";
}

AppStateRenderTarget::AppStateRenderTarget()
    : targetWidth(0)
    , targetHeight(0)
    , isEffectEnabled(true)
    , spinnerObject(nullptr)
{
}

AppStateRenderTarget::~AppStateRenderTarget()
{
}

void AppStateRenderTarget::Init()
{
    CC::CameraManager::Get()->SetActiveCamera("CameraFree");

    CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateRenderTarget::Init()");

    CC::InputActionMap& actionMap = CC::InputManager::Get()->GetActionMap();
    actionMap.CreateContext("AppStateRenderTarget");
    actionMap.RegisterAction(CC::ActionDef(ToggleEffect, "RenderTarget_ToggleEffect", CC::InputTrigger::GamepadFaceRight));
    actionMap.RegisterAction(CC::ActionDef(Back,         "RenderTarget_Back",         CC::InputTrigger::GamepadStart));
    actionMap.SetContext("AppStateRenderTarget");

    CC::InputManager::Get()->SetInteractionMode(CC::InteractionMode::World);
    CC::InputManager::Get()->LockMouseCursor(true);

    SceneInit();

    int width  = 0;
    int height = 0;
    CC::RenderManager::Get()->GetWindowSize(width, height);
    CreateOffscreenTarget(width, height);

    CC::Material* postMaterial = CC::MaterialManager::Get()->GetMaterial(POST_MATERIAL_NAME);
    isEffectEnabled = true;
    postMaterial->SetUniform("effectAmount", 1.0f);

    CC::RenderManager::Get()->SetOffscreenScenePass(renderTarget, colorTexture, postMaterial);
}

void AppStateRenderTarget::CreateOffscreenTarget(int width, int height)
{
    CC::Gfx::RenderApi* gfxApi = CC::Gfx::RenderApi::Get();

    CC::Gfx::TextureDescription colorDesc;
    colorDesc.width          = width;
    colorDesc.height         = height;
    colorDesc.format         = CC::Gfx::TextureFormat::Rgba8Unorm;
    colorDesc.isRenderTarget = true;
    colorDesc.debugName      = "AppStateRenderTarget::Color";
    colorTexture = gfxApi->CreateTexture(colorDesc);

    CC::Gfx::TextureDescription depthDesc;
    depthDesc.width          = width;
    depthDesc.height         = height;
    depthDesc.format         = CC::Gfx::TextureFormat::Depth24Stencil8;
    depthDesc.isRenderTarget = true;
    depthDesc.debugName      = "AppStateRenderTarget::Depth";
    depthTexture = gfxApi->CreateTexture(depthDesc);

    CC::Gfx::RenderTargetDescription rtDesc;
    rtDesc.width                = width;
    rtDesc.height               = height;
    rtDesc.colorAttachmentCount = 1;
    rtDesc.colorAttachments[0].texture       = colorTexture;
    rtDesc.colorAttachments[0].loadOp        = CC::Gfx::LoadOp::Clear;
    rtDesc.colorAttachments[0].storeOp       = CC::Gfx::StoreOp::Store;
    rtDesc.colorAttachments[0].clearColor[0] = 0.10f;
    rtDesc.colorAttachments[0].clearColor[1] = 0.10f;
    rtDesc.colorAttachments[0].clearColor[2] = 0.12f;
    rtDesc.colorAttachments[0].clearColor[3] = 1.0f;
    rtDesc.hasDepthStencil                   = true;
    rtDesc.depthStencilAttachment.texture    = depthTexture;
    rtDesc.depthStencilAttachment.loadOp     = CC::Gfx::LoadOp::Clear;
    rtDesc.depthStencilAttachment.storeOp    = CC::Gfx::StoreOp::DontCare;
    rtDesc.debugName                         = "AppStateRenderTarget::RT";
    renderTarget = gfxApi->CreateRenderTarget(rtDesc);

    targetWidth  = width;
    targetHeight = height;
}

void AppStateRenderTarget::DestroyOffscreenTarget()
{
    CC::Gfx::RenderApi* gfxApi = CC::Gfx::RenderApi::Get();

    if (renderTarget.IsValid())
    {
        gfxApi->DestroyRenderTarget(renderTarget);
        renderTarget = CC::Gfx::RenderTargetHandle();
    }
    if (colorTexture.IsValid())
    {
        gfxApi->DestroyTexture(colorTexture);
        colorTexture = CC::Gfx::TextureHandle();
    }
    if (depthTexture.IsValid())
    {
        gfxApi->DestroyTexture(depthTexture);
        depthTexture = CC::Gfx::TextureHandle();
    }
}

void AppStateRenderTarget::SceneInit()
{
    CC::SceneHierarchy::Get()->LoadFromFile("Data/Scenes/ExampleScene.yaml");

    spinnerObject = new CC::SceneObject("RenderTargetSpinner");
    spinnerObject->AddComponent(new CC::RenderableSphere(CC::MaterialManager::Get()->GetMaterial("BlueTestPattern")));
    spinnerObject->AddComponent(new CC::RotateRandom());
    spinnerObject->GetTransform().SetPosition(CC::Vector3(0, 3, -4));
    CC::SceneHierarchy::Get()->AddRootObject(spinnerObject);

    CC::SceneHierarchy::Get()->Init();
    CC::SceneHierarchy::Get()->SetEnabled(true);
}

void AppStateRenderTarget::SceneShutdown()
{
    CC::SceneHierarchy::Get()->Shutdown();
    spinnerObject = nullptr;
    CC::SceneHierarchy::Get()->Clear();
}

void AppStateRenderTarget::Update()
{
    CC::InputManager* input = CC::InputManager::Get();

    if (input->EdgePositive(ToggleEffect))
    {
        isEffectEnabled = !isEffectEnabled;
        CC::MaterialManager::Get()->GetMaterial(POST_MATERIAL_NAME)->SetUniform(
            "effectAmount", isEffectEnabled ? 1.0f : 0.0f);
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateRenderTarget: invert effect %s",
            isEffectEnabled ? "on" : "off");
    }

    if (input->EdgePositive(Back))
    {
        CC::StateMachine::Get()->GotoState("Boot");
    }

    // Recreate the target when the framebuffer size changes (fullscreen
    // toggle, window resize). Also the manual leak check: DestroyRenderTarget
    // must free its FBO before the replacement is built.
    int width  = 0;
    int height = 0;
    CC::RenderManager::Get()->GetWindowSize(width, height);
    bool sizeChanged = (width != targetWidth || height != targetHeight);
    bool reloadRequested = input->EdgePositive(CC::InputAction::DevReloadScene);
    if ((sizeChanged || reloadRequested) && width > 0 && height > 0)
    {
        CC::RenderManager::Get()->ClearOffscreenScenePass();
        DestroyOffscreenTarget();
        CreateOffscreenTarget(width, height);
        CC::RenderManager::Get()->SetOffscreenScenePass(
            renderTarget, colorTexture, CC::MaterialManager::Get()->GetMaterial(POST_MATERIAL_NAME));
    }
}

void AppStateRenderTarget::Shutdown()
{
    CC::RenderManager::Get()->ClearOffscreenScenePass();
    DestroyOffscreenTarget();
    SceneShutdown();
}
