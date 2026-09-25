#include "XrGrabbable.h"

#include <algorithm>

#include "ComponentFactory.h"

namespace CC
{
    std::vector<XrGrabbable*> XrGrabbable::registered;

    XrGrabbable::XrGrabbable()
    {
    }

    XrGrabbable::~XrGrabbable()
    {
        // A grabbable deleted without its Shutdown running would otherwise
        // leave the hands holding a dangling address.
        Unregister();
    }

    void XrGrabbable::Init()
    {
        if (!isRegistered)
        {
            registered.push_back(this);
            isRegistered = true;
        }
    }

    void XrGrabbable::Shutdown()
    {
        Unregister();
    }

    const std::vector<XrGrabbable*>& XrGrabbable::GetRegistered()
    {
        return registered;
    }

    void XrGrabbable::Unregister()
    {
        if (isRegistered)
        {
            registered.erase(std::remove(registered.begin(), registered.end(), this), registered.end());
            isRegistered = false;
        }
    }
}

// ========================
// Scene file registration
// ========================
static CC::Component* CreateXrGrabbable(ryml::ConstNodeRef componentData)
{
    (void)componentData;
    return new CC::XrGrabbable();
}

static CC::ComponentRegistrar xrGrabbableRegistrar("XrGrabbable", CreateXrGrabbable);
