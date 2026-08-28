#include "Component.h"
#include "CCAssert.h"

namespace CC
{
    Component::Component()
        : owner(nullptr)
        , isEnabled(true)
        , isPauseable(false)
    {
    }

    Component::~Component()
    {
    }

    void Component::SetOwner(SceneObject* _owner)
    {
        CC_ASSERT(_owner != nullptr, "Cannot set null owner on Component");
        owner = _owner;
    }
}
