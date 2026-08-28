#include "RotateRandom.h"
#include "ComponentFactory.h"
#include "SceneObject.h"
#include "FrameTimer.h"
#include <cstdlib>
#include <cmath>

namespace CC
{
    RotateRandom::RotateRandom()
        : rotationSpeed(0.0f, 0.0f, 0.0f)
        , rotationMultiplier(0.0f, 0.0f, 0.0f)
    {
        SetPauseable(true);
    }

    RotateRandom::~RotateRandom()
    {
    }

    void RotateRandom::Init()
    {
        rotationSpeed.x = 0.2f + (rand() / (float)RAND_MAX) * 0.6f;
        rotationSpeed.y = 0.2f + (rand() / (float)RAND_MAX) * 0.6f;
        rotationSpeed.z = 0.2f + (rand() / (float)RAND_MAX) * 0.6f;

        rotationMultiplier.x = 180.0f + (rand() / (float)RAND_MAX) * 180.0f;
        rotationMultiplier.y = 180.0f + (rand() / (float)RAND_MAX) * 180.0f;
        rotationMultiplier.z = 180.0f + (rand() / (float)RAND_MAX) * 180.0f;
    }

    void RotateRandom::Update()
    {
        if (owner == nullptr)
        {
            return;
        }

        float time = FrameTimer::Get()->TimeSinceStartup();

        Vector3 rotation(
            rotationMultiplier.x * sinf(time * rotationSpeed.x),
            rotationMultiplier.y * sinf(time * rotationSpeed.y),
            rotationMultiplier.z * sinf(time * rotationSpeed.z)
        );

        owner->GetTransform().SetRotation(rotation);
    }
}

// ========================
// Factory Registration
// ========================

static CC::Component* CreateRotateRandom(ryml::ConstNodeRef componentData)
{
    return new CC::RotateRandom();
}

static CC::ComponentRegistrar rotateRandomRegistrar("RotateRandom", CreateRotateRandom);
