#ifndef APPSTATERENDERTARGET_H
#define APPSTATERENDERTARGET_H

#include "StateMachineState.h"
#include "InputActionMap.h"
#include "GfxHandles.h"

namespace CC
{
    class SceneObject;
}

// Demo / manual regression state for offscreen render targets (AGD-0080).
// Renders the scene into an offscreen colour target, then draws a fullscreen
// pass that samples it with a colour-invert effect into the backbuffer.
// Toggling the effect off must reproduce the direct render path (MSAA on
// scene edges aside — offscreen targets are single-sample).
class AppStateRenderTarget : public StateMachineState
{
public:
    AppStateRenderTarget();
    virtual ~AppStateRenderTarget();

    virtual void Init() override;
    virtual void Update() override;
    virtual void Shutdown() override;

private:
    enum RenderTargetActions
    {
        ToggleEffect = CC::InputAction::GameActionStart,
        Back,
        RenderTargetActionMax
    };

    void CreateOffscreenTarget(int width, int height);
    void DestroyOffscreenTarget();
    void SceneInit();
    void SceneShutdown();

    CC::Gfx::RenderTargetHandle renderTarget;
    CC::Gfx::TextureHandle      colorTexture;
    CC::Gfx::TextureHandle      depthTexture;
    int                         targetWidth;
    int                         targetHeight;

    bool                        isEffectEnabled;

    CC::SceneObject*            spinnerObject;
};

#endif // APPSTATERENDERTARGET_H
