#ifndef DIRECTIONALLIGHT_H
#define DIRECTIONALLIGHT_H

#include <string>
#include "CCVector3.h"

namespace CC
{
    class LightDirectional
    {
    public:
        LightDirectional(const std::string& name, const Vector3& direction, const Vector3& color);
        virtual ~LightDirectional();

        void SetDirection(const Vector3& direction);
        void SetColor(const Vector3& color);

        const float GetIntensity() const { return intensity; }
        const Vector3& GetDirection() const { return direction; }
        const Vector3& GetColor() const { return color; }
        const std::string& GetName() const { return name; }

    private:
        std::string name;
        float intensity;
        Vector3 direction;
        Vector3 color;
    };
}

#endif // DIRECTIONALLIGHT_H