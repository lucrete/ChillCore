#ifndef SCENEOBJECT_H
#define SCENEOBJECT_H

#include <string>
#include <vector>
#include "Transform.h"

namespace CC
{
    class Component;

    class SceneObject
    {
    public:
        SceneObject();
        SceneObject(const std::string& _name);
        virtual ~SceneObject();

        // Lifecycle
        void Init();
        void Update();
        void Shutdown();

        // Hierarchy
        void SetParent(SceneObject* _parent);
        SceneObject* GetParent() const { return parent; }
        void AddChild(SceneObject* child);
        void RemoveChild(SceneObject* child);
        const std::vector<SceneObject*>& GetChildren() const { return children; }

        // Transform
        Transform& GetTransform() { return transform; }
        const Transform& GetTransform() const { return transform; }
        void GetWorldMatrix(Mat4x4& worldMatrix) const;
        Vector3 GetWorldPosition() const;

        // Components
        void AddComponent(Component* component);
        void RemoveComponent(Component* component);
        const std::vector<Component*>& GetComponents() const { return components; }

        template<typename T>
        T* GetComponent() const
        {
            for (Component* component : components)
            {
                T* typedComponent = dynamic_cast<T*>(component);
                if (typedComponent != nullptr)
                {
                    return typedComponent;
                }
            }
            return nullptr;
        }

        // Identity
        void SetName(const std::string& _name) { name = _name; }
        const std::string& GetName() const { return name; }
        void SetEnabled(bool _isEnabled) { isEnabled = _isEnabled; }
        bool IsEnabled() const { return isEnabled; }

    protected:
        Transform transform;

    private:
        std::string name;
        bool isEnabled = true;
        SceneObject* parent = nullptr;
        std::vector<SceneObject*> children;
        std::vector<Component*> components;
    };
}

#endif // SCENEOBJECT_H
