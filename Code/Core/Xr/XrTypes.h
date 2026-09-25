#ifndef XRTYPES_H
#define XRTYPES_H

#include "CCVector3.h"
#include "CCQuaternion.h"
#include "GfxHandles.h"

namespace CC
{
    // ========================
    // XR value types
    // ========================
    //
    // What the XR subsystem publishes to the rest of the engine. No OpenXR
    // type appears here or anywhere above Core/Xr: poses arrive as engine
    // vectors and quaternions, and frustum edges as tangents.

    // Position and orientation of a tracked thing. isTracked is false while
    // the runtime has no confident answer, in which case the pose holds the
    // last one it did have and should not be treated as current.
    struct TrackedPose
    {
        Vector3    position;
        Quaternion orientation;
        bool       isTracked = false;
    };

    enum class XrHand
    {
        Left = 0,
        Right,
        Max
    };

    // Grip sits in the fist and is what a held object follows. Aim points
    // where the controller points and is what a ray starts from. The runtime
    // defines both per controller, so neither is derivable from the other.
    enum class XrPoseKind
    {
        Grip = 0,
        Aim,
        Max
    };

    // One eye for one frame: where it is, what it sees, and what it renders
    // into. The four angles are the frustum half-angles in radians, signed
    // from the view axis, and are not symmetric.
    struct XrEyeView
    {
        TrackedPose pose;

        float angleLeft  = 0.0f;
        float angleRight = 0.0f;
        float angleUp    = 0.0f;
        float angleDown  = 0.0f;

        // Engine-owned, multisampled, rendered into. Resolves into
        // sceneColorTexture when the pass ends.
        Gfx::RenderTargetHandle sceneTarget;
        Gfx::TextureHandle      sceneColorTexture;

        // The runtime's swapchain image acquired this frame, wrapped as a
        // render target. The post-process pass resolves into it.
        Gfx::RenderTargetHandle swapchainTarget;
        Gfx::TextureHandle      swapchainTexture;

        int width  = 0;
        int height = 0;
    };
}

#endif // XRTYPES_H
