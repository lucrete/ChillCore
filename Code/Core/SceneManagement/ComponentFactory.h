#ifndef COMPONENTFACTORY_H
#define COMPONENTFACTORY_H

// Suppress macro redefinition warnings from Windows headers conflicting with GLFW
#pragma warning(push)
#pragma warning(disable: 4005)
#include <rapidyaml-0.10.0.hpp>
#pragma warning(pop)

#include <string>
#include <map>

// Helper function for extracting string values from YAML nodes
inline std::string NodeToString(ryml::ConstNodeRef node)
{
    if (!node.has_val())
    {
        return "";
    }
    c4::csubstr val = node.val();
    return std::string(val.data(), val.size());
}

namespace CC
{
    class Component;

    typedef Component* (*ComponentFactoryFunc)(ryml::ConstNodeRef componentData);

    class ComponentFactory
    {
    public:
        static ComponentFactory* Get();

        void RegisterComponent(const std::string& typeName, ComponentFactoryFunc factoryFunc);
        bool HasComponent(const std::string& typeName) const;
        Component* CreateComponent(const std::string& typeName, ryml::ConstNodeRef componentData);

    private:
        ComponentFactory();
        std::map<std::string, ComponentFactoryFunc> factoryMap;
    };

    // Helper for static registration
    struct ComponentRegistrar
    {
        ComponentRegistrar(const char* typeName, ComponentFactoryFunc func)
        {
            ComponentFactory::Get()->RegisterComponent(typeName, func);
        }
    };
}

#endif // COMPONENTFACTORY_H
