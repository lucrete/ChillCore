#ifndef UIPOINTERTARGET_H
#define UIPOINTERTARGET_H

#include "CCVector3.h"

namespace CC
{
    // Something a world pointer can reach. It answers whether a ray reaches
    // it and at what distance, and then what the hit means.
    //
    // Deliberately not a plane. A UI element that is a reactive 3D mesh
    // rather than a region on a flat panel implements this same interface
    // against its own geometry and drives its own hover and press with no
    // surface involved. Simplifying this back to a plane would mean
    // rebuilding the router for it.
    class UiPointerTarget
    {
    public:
        virtual ~UiPointerTarget() = default;

        // Distance along the ray at which it reaches this target. False
        // where it does not.
        virtual bool IntersectPointerRay(const Vector3& origin, const Vector3& direction,
                                         float maxDistance, float& outDistance) const = 0;

        // Called for the nearest target a pointer reached, once per frame.
        virtual void OnPointerHit(int pointerId, const Vector3& origin, const Vector3& direction,
                                  float distance, bool isPressed) = 0;

        // Called when a pointer that was on this target no longer is,
        // whether it moved on to another or stopped submitting.
        virtual void OnPointerLeft(int pointerId) = 0;
    };
}

#endif // UIPOINTERTARGET_H
