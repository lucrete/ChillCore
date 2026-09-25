#ifndef UIWORLDPANEL_H
#define UIWORLDPANEL_H

#include <string>

#include "CCVector3.h"
#include "Component.h"
#include "UiPointerRouter.h"
#include "UiPointerTarget.h"

namespace CC
{
    class Material;
    class UiSurface;

    // A UI panel standing in the world: its own surface, its own target, and
    // the quad that shows it. Placement, parenting, lifetime and hiding are
    // the scene's answers rather than new ones.
    //
    // The panel's size in metres is its owner's transform scale; the surface
    // knows only pixels. Hiding one is SetEnabled, which skips its update,
    // hit test and render while keeping its target allocated.
    class UiWorldPanel : public Component, public UiPointerTarget
    {
    public:
        UiWorldPanel() = delete;
        UiWorldPanel(const std::string& surfaceName, int textureWidth, int textureHeight);
        virtual ~UiWorldPanel();

        virtual const char* GetTypeName() const override { return "UiWorldPanel"; }

        // Takes the surface with it, so a hidden panel submits no work while
        // keeping its target allocated.
        virtual void SetEnabled(bool _isEnabled) override;

        virtual void Init() override;
        virtual void Update() override;
        virtual void Shutdown() override;

        // Where screens are registered and elements resolved. Valid from
        // Init onwards.
        UiSurface* GetSurface() const { return surface; }

        // Whether this pointer is currently on the panel. What the router
        // decided, rather than a second intersection test.
        bool IsPointerOn(int pointerId) const;

        // ========================
        // UiPointerTarget
        // ========================
        virtual bool IntersectPointerRay(const Vector3& origin, const Vector3& direction,
                                         float maxDistance, float& outDistance) const override;
        virtual void OnPointerHit(int pointerId, const Vector3& origin, const Vector3& direction,
                                  float distance, bool isPressed) override;
        virtual void OnPointerLeft(int pointerId) override;
        virtual const UiSurface* GetPointerSurface() const override { return surface; }

    private:
        std::string surfaceName;
        int         textureWidth;
        int         textureHeight;

        UiSurface* surface = nullptr;
        Material*  material = nullptr;

        // The quad's plane in world space, rebuilt each frame from the
        // owner's transform so a panel that moves stays hit-testable.
        Vector3 planeOrigin;
        Vector3 planeNormal;
        Vector3 planeRight;
        Vector3 planeUp;
        float   halfWidthMetres  = 0.5f;
        float   halfHeightMetres = 0.5f;

        bool isPointerOn[UiPointerRouter::MAX_POINTERS] = {};

        void RebuildPlane();

        // Converts a world-space hit on the plane into surface pixels.
        // False where the hit falls outside the quad's bounds.
        bool HitToPixels(const Vector3& hit, float& outPixelX, float& outPixelY) const;
    };
}

#endif // UIWORLDPANEL_H
