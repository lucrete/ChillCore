#ifndef ROTATERANDOM_H
#define ROTATERANDOM_H

#include "Component.h"
#include "CCVector3.h"

namespace CC
{
    class RotateRandom : public Component
    {
    public:
        RotateRandom();
        virtual ~RotateRandom();

        virtual void Init() override;
        virtual void Update() override;

        virtual const char* GetTypeName() const override { return "RotateRandom"; }

    private:
        Vector3 rotationSpeed;
        Vector3 rotationMultiplier;
    };
}

#endif // ROTATERANDOM_H
