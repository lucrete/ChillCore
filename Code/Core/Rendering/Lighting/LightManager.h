#ifndef LIGHTMANAGER_H
#define LIGHTMANAGER_H

#include "LightDirectional.h"
#include <map>
#include <string>
#include <memory>

namespace CC
{
    class LightManager
    {
    public:
        LightManager();
        virtual ~LightManager();

        static LightManager* Get();

        LightDirectional* CreateLight(const std::string& name, const Vector3& direction, const Vector3& color);
        LightDirectional* GetLight(const std::string& name);
        float GetAmbientLightIntensity() const { return ambientLightInstensity; }
        Vector3 GetAmbientLightColor() const { return ambientLightColor; }
        void RemoveLight(const std::string& name);
        void Clear();

    private:
        static LightManager* instance;
        std::map<std::string, std::unique_ptr<LightDirectional>> lights;
        float ambientLightInstensity;
        Vector3 ambientLightColor;
    };
}

#endif // LIGHTMANAGER_H