#include "ComponentFactory.h"
#include "Component.h"
#include "PrintManager.h"

namespace CC
{
    ComponentFactory* ComponentFactory::Get()
    {
        static ComponentFactory* instance = new ComponentFactory();
        return instance;
    }

    ComponentFactory::ComponentFactory()
    {
    }

    void ComponentFactory::RegisterComponent(const std::string& typeName, ComponentFactoryFunc factoryFunc)
    {
        factoryMap[typeName] = factoryFunc;
    }

    bool ComponentFactory::HasComponent(const std::string& typeName) const
    {
        return factoryMap.find(typeName) != factoryMap.end();
    }

    Component* ComponentFactory::CreateComponent(const std::string& typeName, ryml::ConstNodeRef componentData)
    {
        auto it = factoryMap.find(typeName);
        if (it == factoryMap.end())
        {
            CCPrint(PrintManager::CHANNEL_WARN, "ComponentFactory: Unknown component type '%s'", typeName.c_str());
            return nullptr;
        }
        return it->second(componentData);
    }
}
