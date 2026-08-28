#ifndef SCENEHIERARCHY_H
#define SCENEHIERARCHY_H

#include <string>
#include <vector>
#include <unordered_map>

namespace CC
{
    class SceneObject;

    class SceneHierarchy
    {
    public:
        SceneHierarchy();
        virtual ~SceneHierarchy();
        static SceneHierarchy* Get();

        void Init();
        void Update();
        void Shutdown();

        void SetEnabled(bool _isEnabled) { isEnabled = _isEnabled; }
        bool IsEnabled() const { return isEnabled; }

        void SetPaused(bool _isPaused) { isPaused = _isPaused; }
        bool IsPaused() const { return isPaused; }

        void AddRootObject(SceneObject* object);
        void RemoveRootObject(SceneObject* object);
        const std::vector<SceneObject*>& GetRootObjects() const { return rootObjects; }

        bool LoadFromFile(const std::string& filePath);
        void Clear();

        SceneObject* FindObjectByName(const std::string& name) const;

        int GetVersion() const { return version; }

    private:
        static SceneHierarchy* instance;

        void RegisterObjectRecursive(SceneObject* object);
        void UnregisterObjectRecursive(SceneObject* object);

        bool isEnabled = true;
        bool isPaused = false;
        int version = 0;
        std::vector<SceneObject*> rootObjects;
        std::unordered_map<std::string, SceneObject*> objectLookup;
    };
}

#endif // SCENEHIERARCHY_H
