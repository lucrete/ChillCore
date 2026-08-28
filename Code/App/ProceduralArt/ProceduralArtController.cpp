#include "ProceduralArtController.h"
#include "RenderManager.h"
#include "PrintManager.h"
#include "MaterialManager.h"
#include "DevUi.h"
#include <cstdio>

ProceduralArtController::ProceduralArtController()
    : center(0.0f, 0.0f)
    , scale(4.0f)
    , velocityMove(1)
    , velocityScale(3)
    , currentEffectIndex(0)
{
    renderableFullscreenQuad = CC::RenderManager::Get()->GetRenderableFullscreenQuad();
    renderableFullscreenQuad->SetOnRenderCallback([&](CC::Material& material) {OnRender(material); });

    InitEffects();

    CC::InputActionMap& actionMap = CC::InputManager::Get()->GetActionMap();
    actionMap.CreateContext("ProceduralArtControls");
    actionMap.RegisterAction(CC::ActionDef(MoveLeft, "Fractal2D_MoveLeft", CC::InputTrigger::GamepadDpadLeft));
    actionMap.RegisterAction(CC::ActionDef(MoveRight, "Fractal2D_MoveRight", CC::InputTrigger::GamepadDpadRight));
    actionMap.RegisterAction(CC::ActionDef(MoveUp, "Fractal2D_MoveUp", CC::InputTrigger::GamepadDpadUp));
    actionMap.RegisterAction(CC::ActionDef(MoveDown, "Fractal2D_MoveDown", CC::InputTrigger::GamepadDpadDown));
    actionMap.RegisterAction(CC::ActionDef(ZoomIn, "Fractal2D_ZoomIn", CC::InputTrigger::GamepadBumperRight));
    actionMap.RegisterAction(CC::ActionDef(ZoomOut, "Fractal2D_ZoomOut", CC::InputTrigger::GamepadBumperLeft));
    actionMap.RegisterAction(CC::ActionDef(NextEffect, "ProcArt_NextEffect", CC::InputTrigger::GamepadFaceRight));
    actionMap.RegisterAction(CC::ActionDef(PrevEffect, "ProcArt_PrevEffect", CC::InputTrigger::GamepadFaceLeft));
}

void ProceduralArtController::InitEffects()
{
    CC::MaterialManager* matMgr = CC::MaterialManager::Get();

    effects.emplace_back(matMgr->GetMaterial("ProcArt_Fractal2d"), "Fractal2d", true);
    effects.emplace_back(matMgr->GetMaterial("ProcArt_GradientViewer"), "GradientViewer", false);
    effects.emplace_back(matMgr->GetMaterial("ProcArt_UnitCircleRipples"), "UnitCircleRipples", false);
    effects.emplace_back(matMgr->GetMaterial("ProcArt_RadialWaves"), "RadialWaves", false);
    effects.emplace_back(matMgr->GetMaterial("ProcArt_TrigWaves"), "TrigWaves", false);

    // Set initial material
    if (!effects.empty())
    {
        renderableFullscreenQuad->SetMaterial(effects[currentEffectIndex].material);
    }
}

void ProceduralArtController::Init()
{
    CC::InputManager::Get()->GetActionMap().SetContext("ProceduralArtControls");
}

ProceduralArtController::~ProceduralArtController()
{
}

void ProceduralArtController::Shutdown()
{
    CC::DevUi::Get()->SetContextText("");
}

void ProceduralArtController::Update()
{
    float deltaSeconds = 0.016f;

    CC::InputManager* input = CC::InputManager::Get();

    // Effect cycling
    if (input->EdgePositive(NextEffect))
    {
        CycleEffect(1);
    }
    else if (input->EdgePositive(PrevEffect))
    {
        CycleEffect(-1);
    }

    // Custom controls only for effects that use them (e.g., Fractal2d)
    if (!effects.empty() && effects[currentEffectIndex].useCustomControls)
    {
        float deltaScale = velocityScale * deltaSeconds;
        if (input->IsPressed(ZoomIn))
        {
            scale *= 0.99f;
        }
        else if (input->IsPressed(ZoomOut))
        {
            scale *= 1.01f;
        }

        float deltaMove = velocityMove * deltaSeconds * scale;

        if (input->IsPressed(MoveLeft))
        {
            center.x -= deltaMove;
        }
        else if (input->IsPressed(MoveRight))
        {
            center.x += deltaMove;
        }

        if (input->IsPressed(MoveUp))
        {
            center.y += deltaMove;
        }
        else if (input->IsPressed(MoveDown))
        {
            center.y -= deltaMove;
        }

        char contextBuffer[128];
        snprintf(contextBuffer, sizeof(contextBuffer), "Center: (%f, %f), Scale: %f", center.x, center.y, scale);
        CC::DevUi::Get()->SetContextText(contextBuffer);
    }
}

void ProceduralArtController::CycleEffect(int direction)
{
    if (effects.empty()) return;

    currentEffectIndex = (currentEffectIndex + direction + static_cast<int>(effects.size())) % static_cast<int>(effects.size());
    renderableFullscreenQuad->SetMaterial(effects[currentEffectIndex].material);

    CC::DevUi::Get()->SetContextText("");

    CC::PrintManager::Get()->Print(CC::PrintManager::CHANNEL_ALWAYS, "Effect: %s", effects[currentEffectIndex].name.c_str());
}

void ProceduralArtController::OnRender(CC::Material& material)
{
    if (!effects.empty() && effects[currentEffectIndex].useCustomControls)
    {
        material.SetUniform("center", center);
        material.SetUniform("scale", scale);
    }
}