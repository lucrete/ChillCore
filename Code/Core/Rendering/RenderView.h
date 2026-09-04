#ifndef RENDERVIEW_H
#define RENDERVIEW_H

#include "GfxHandles.h"

namespace CC
{
    class CameraBase;

    // One scene render: which camera, which target it draws into, and where
    // the post-process pass resolves it to. A flat frame is one view. Stereo
    // is two, differing only in camera and targets, submitted one after the
    // other over the same renderable list.
    struct RenderView
    {
        // Null means the camera the CameraManager already has active, which
        // is what a flat frame wants. A view naming its own camera activates
        // it and rebuilds its matrices before the pass opens.
        CameraBase* camera = nullptr;

        // Target the scene pass draws into. Multisampled where the scene is
        // antialiased; resolved into sceneColorTexture when the pass ends.
        Gfx::RenderTargetHandle sceneTarget;

        // Colour attachment of sceneTarget, sampled by the post-process
        // pass. Invalid where there is no offscreen target, in which case
        // the scene has already presented itself and post is skipped.
        Gfx::TextureHandle sceneColorTexture;

        // Where the post-process pass writes its display-referred result.
        Gfx::RenderTargetHandle outputTarget;

        // Colour attachment of outputTarget, where one can be sampled. The
        // desktop mirror reads the first view's through it. Invalid for the
        // backbuffer, which is not a sampleable texture.
        Gfx::TextureHandle outputColorTexture;

        int width = 0;
        int height = 0;
    };
}

#endif // RENDERVIEW_H
