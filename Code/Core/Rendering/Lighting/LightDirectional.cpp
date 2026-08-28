#include "LightDirectional.h"
#include "CCMath.h"

namespace CC
{
    LightDirectional::LightDirectional(const std::string& name, const Vector3& _direction, const Vector3& _color)
        : name(name)
        , intensity(1.0f)
        , color(_color)
    {
        SetDirection(_direction);
    }

    LightDirectional::~LightDirectional()
    {
    }

    void LightDirectional::SetDirection(const Vector3& _direction)
    {
        // Normalize the direction vector
        direction = _direction;
        direction.Normalize();
    }

    void LightDirectional::SetColor(const Vector3& _color)
    {
        color = _color;
    }
}