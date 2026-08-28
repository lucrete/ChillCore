#include "SceneHierarchy.h"
#include "SceneObject.h"
#include "SceneLoader.h"
#include "CCAssert.h"
#include <algorithm>

namespace CC
{
    SceneHierarchy* SceneHierarchy::instance = nullptr;

    SceneHierarchy::SceneHierarchy()
    {
        CC_ASSERT(instance == nullptr, "SceneHierarchy already created");
        instance = this;
    }

    SceneHierarchy::~SceneHierarchy()
    {
        Clear();
        instance = nullptr;
    }

    SceneHierarchy* SceneHierarchy::Get()
    {
        CC_ASSERT(instance != nullptr, "SceneHierarchy not created yet");
        return instance;
    }

    // ========================
    // Lifecycle
    // ========================

    void SceneHierarchy::Init()
    {
        for (SceneObject* object : rootObjects)
        {
            object->Init();
        }
    }

    void SceneHierarchy::Update()
    {
        if (!isEnabled)
        {
            return;
        }

        for (SceneObject* object : rootObjects)
        {
            object->Update();
        }
    }

    void SceneHierarchy::Shutdown()
    {
        for (auto it = rootObjects.rbegin(); it != rootObjects.rend(); ++it)
        {
            (*it)->Shutdown();
        }
    }

    // ========================
    // Root Object Management
    // ========================

    void SceneHierarchy::AddRootObject(SceneObject* object)
    {
        CC_ASSERT(object != nullptr, "Cannot add null object to SceneHierarchy");
        CC_ASSERT(object->GetParent() == nullptr, "Root objects cannot have a parent");

        auto it = std::find(rootObjects.begin(), rootObjects.end(), object);
        if (it == rootObjects.end())
        {
            rootObjects.push_back(object);
            RegisterObjectRecursive(object);
        }
    }

    void SceneHierarchy::RemoveRootObject(SceneObject* object)
    {
        auto it = std::find(rootObjects.begin(), rootObjects.end(), object);
        if (it != rootObjects.end())
        {
            UnregisterObjectRecursive(object);
            rootObjects.erase(it);
        }
    }

    // ========================
    // Scene Loading
    // ========================

    bool SceneHierarchy::LoadFromFile(const std::string& filePath)
    {
        Clear();
        return SceneLoader::LoadScene(filePath, *this);
    }

    void SceneHierarchy::Clear()
    {
        version++;
        for (SceneObject* object : rootObjects)
        {
            delete object;
        }
        rootObjects.clear();
        objectLookup.clear();
    }

    // ========================
    // Object Lookup
    // ========================

    SceneObject* SceneHierarchy::FindObjectByName(const std::string& name) const
    {
        auto it = objectLookup.find(name);
        if (it != objectLookup.end())
        {
            return it->second;
        }
        return nullptr;
    }

    // ========================
    // Internal Helpers
    // ========================

    void SceneHierarchy::RegisterObjectRecursive(SceneObject* object)
    {
        const std::string& name = object->GetName();
        if (!name.empty())
        {
            objectLookup[name] = object;
        }

        for (SceneObject* child : object->GetChildren())
        {
            RegisterObjectRecursive(child);
        }
    }

    void SceneHierarchy::UnregisterObjectRecursive(SceneObject* object)
    {
        const std::string& name = object->GetName();
        if (!name.empty())
        {
            objectLookup.erase(name);
        }

        for (SceneObject* child : object->GetChildren())
        {
            UnregisterObjectRecursive(child);
        }
    }
}
