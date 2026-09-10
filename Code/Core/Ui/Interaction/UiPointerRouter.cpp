#include "UiPointerRouter.h"

#include "CCAssert.h"

namespace CC
{
    void UiPointerRouter::RegisterTarget(UiPointerTarget* target)
    {
        CC_ASSERT(target != nullptr, "Null pointer target");
        targets.push_back(target);
    }

    void UiPointerRouter::UnregisterTarget(UiPointerTarget* target)
    {
        for (size_t i = 0; i < targets.size(); i++)
        {
            if (targets[i] == target)
            {
                targets.erase(targets.begin() + i);
                break;
            }
        }

        // A target that goes away while a pointer is on it leaves that
        // pointer holding a dangling address otherwise.
        for (int pointer = 0; pointer < MAX_POINTERS; pointer++)
        {
            if (currentTarget[pointer] == target)
            {
                currentTarget[pointer] = nullptr;
            }
        }
    }

    void UiPointerRouter::SubmitRay(int pointerId, const Vector3& origin, const Vector3& direction,
                                    float maxDistance, bool isPressed)
    {
        CC_ASSERT(pointerId >= 0 && pointerId < MAX_POINTERS, "Pointer id out of range");

        PointerSubmission& submission = submissions[pointerId];
        submission.origin      = origin;
        submission.direction   = direction;
        submission.maxDistance = maxDistance;
        submission.isPressed   = isPressed;
        submission.isSubmitted = true;
    }

    void UiPointerRouter::Resolve()
    {
        for (int pointer = 0; pointer < MAX_POINTERS; pointer++)
        {
            PointerSubmission& submission = submissions[pointer];

            UiPointerTarget* nearestTarget = nullptr;
            float nearestDistance = 0.0f;

            if (submission.isSubmitted)
            {
                for (UiPointerTarget* target : targets)
                {
                    float distance = 0.0f;
                    if (target->IntersectPointerRay(submission.origin, submission.direction,
                                                    submission.maxDistance, distance))
                    {
                        if (nearestTarget == nullptr || distance < nearestDistance)
                        {
                            nearestTarget = target;
                            nearestDistance = distance;
                        }
                    }
                }
            }

            if (currentTarget[pointer] != nullptr && currentTarget[pointer] != nearestTarget)
            {
                currentTarget[pointer]->OnPointerLeft(pointer);
            }
            currentTarget[pointer] = nearestTarget;

            if (nearestTarget != nullptr)
            {
                nearestTarget->OnPointerHit(pointer, submission.origin, submission.direction,
                                            nearestDistance, submission.isPressed);
            }

            submission.isSubmitted = false;
        }
    }
}
