#include "SceneObject.h"
#include "Component.h"
#include "SceneHierarchy.h"
#include "CCAssert.h"
#include <algorithm>

namespace CC
{
    SceneObject::SceneObject()
        : name("")
        , isEnabled(true)
        , parent(nullptr)
    {
    }

    SceneObject::SceneObject(const std::string& _name)
        : name(_name)
        , isEnabled(true)
        , parent(nullptr)
    {
    }

    SceneObject::~SceneObject()
    {
        for (Component* component : components)
        {
            delete component;
        }
        components.clear();

        for (SceneObject* child : children)
        {
            delete child;
        }
        children.clear();
    }

    // ========================
    // Lifecycle
    // ========================

    void SceneObject::Init()
    {
        for (Component* component : components)
        {
            component->Init();
        }

        for (SceneObject* child : children)
        {
            child->Init();
        }
    }

    void SceneObject::Update()
    {
        if (!isEnabled)
        {
            return;
        }

        bool hierarchyIsPaused = SceneHierarchy::Get()->IsPaused();

        for (Component* component : components)
        {
            if (component->IsEnabled())
            {
                if (!hierarchyIsPaused || !component->IsPauseable())
                {
                    component->Update();
                }
            }
        }

        for (SceneObject* child : children)
        {
            child->Update();
        }
    }

    void SceneObject::Shutdown()
    {
        for (auto it = children.rbegin(); it != children.rend(); ++it)
        {
            (*it)->Shutdown();
        }

        for (auto it = components.rbegin(); it != components.rend(); ++it)
        {
            (*it)->Shutdown();
        }
    }

    // ========================
    // Hierarchy
    // ========================

    void SceneObject::SetParent(SceneObject* _parent)
    {
        if (parent != nullptr)
        {
            parent->RemoveChild(this);
        }

        parent = _parent;

        if (parent != nullptr)
        {
            parent->AddChild(this);
        }
    }

    void SceneObject::AddChild(SceneObject* child)
    {
        CC_ASSERT(child != nullptr, "Cannot add null child to SceneObject");

        auto it = std::find(children.begin(), children.end(), child);
        if (it == children.end())
        {
            children.push_back(child);
            if (child->parent != this)
            {
                child->parent = this;
            }
        }
    }

    void SceneObject::RemoveChild(SceneObject* child)
    {
        auto it = std::find(children.begin(), children.end(), child);
        if (it != children.end())
        {
            children.erase(it);
            child->parent = nullptr;
        }
    }

    // ========================
    // Transform
    // ========================

    void SceneObject::GetWorldMatrix(Mat4x4& worldMatrix) const
    {
        transform.GetModelMatrix(worldMatrix);

        if (parent != nullptr)
        {
            Mat4x4 parentWorld;
            parent->GetWorldMatrix(parentWorld);
            worldMatrix = parentWorld * worldMatrix;
        }
    }

    Vector3 SceneObject::GetWorldPosition() const
    {
        Mat4x4 worldMatrix;
        GetWorldMatrix(worldMatrix);
        return Vector3(worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2]);
    }

    // ========================
    // Components
    // ========================

    void SceneObject::AddComponent(Component* component)
    {
        CC_ASSERT(component != nullptr, "Cannot add null component to SceneObject");
        components.push_back(component);
        component->SetOwner(this);
    }

    void SceneObject::RemoveComponent(Component* component)
    {
        auto it = std::find(components.begin(), components.end(), component);
        if (it != components.end())
        {
            components.erase(it);
        }
    }
}
