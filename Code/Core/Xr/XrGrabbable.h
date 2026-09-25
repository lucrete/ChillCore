#ifndef XRGRABBABLE_H
#define XRGRABBABLE_H

#include <vector>

#include "Component.h"

namespace CC
{
    // Marks its owner as something the XR hands can pick up. Registers while
    // initialised, the way a world panel registers with the pointer router,
    // so the hands find grabbables without knowing the scene.
    class XrGrabbable : public Component
    {
    public:
        XrGrabbable();
        virtual ~XrGrabbable();

        virtual const char* GetTypeName() const override { return "XrGrabbable"; }

        virtual void Init() override;
        virtual void Shutdown() override;

        static const std::vector<XrGrabbable*>& GetRegistered();

    private:
        static std::vector<XrGrabbable*> registered;

        bool isRegistered = false;

        void Unregister();
    };
}

#endif // XRGRABBABLE_H
