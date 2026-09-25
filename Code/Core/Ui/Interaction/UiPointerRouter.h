#ifndef UIPOINTERROUTER_H
#define UIPOINTERROUTER_H

#include <vector>

#include "CCVector3.h"
#include "UiPointerTarget.h"

namespace CC
{
    // Resolves world pointers to targets. A ray can reach several panels, so
    // something has to take the nearest. It holds no geometry of its own and
    // only compares distances, among the targets the Headset domain's input
    // target allows: every target under Scene, one surface's under Surface.
    class UiPointerRouter
    {
    public:
        // How many world pointers can be live at once. Two hands is the
        // case that exists; the rest is headroom.
        static const int MAX_POINTERS = 4;

        void RegisterTarget(UiPointerTarget* target);
        void UnregisterTarget(UiPointerTarget* target);

        // Offers one pointer's ray for this frame. Nothing is dispatched
        // until Resolve runs, so every target has been offered the ray
        // before the nearest is chosen.
        void SubmitRay(int pointerId, const Vector3& origin, const Vector3& direction,
                       float maxDistance, bool isPressed);

        // Dispatches each submitted pointer to the nearest target it reached
        // and releases the targets of pointers that submitted nothing.
        void Resolve();

        // Whether this pointer reached a target when it was last resolved.
        bool IsPointerOnTarget(int pointerId) const;

    private:
        struct PointerSubmission
        {
            Vector3 origin;
            Vector3 direction;
            float   maxDistance = 0.0f;
            bool    isPressed   = false;
            bool    isSubmitted = false;
        };

        std::vector<UiPointerTarget*> targets;
        PointerSubmission             submissions[MAX_POINTERS];
        UiPointerTarget*              currentTarget[MAX_POINTERS] = {};
    };
}

#endif // UIPOINTERROUTER_H
