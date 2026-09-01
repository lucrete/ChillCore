#ifndef SCENELOADER_H
#define SCENELOADER_H

// Suppress macro redefinition warnings from Windows headers conflicting with GLFW
#pragma warning(push)
#pragma warning(disable: 4005)
#include <rapidyaml-0.10.0.hpp>
#pragma warning(pop)

#include <string>

namespace CC
{
    class SceneHierarchy;
    class SceneObject;

    class SceneLoader
    {
    public:
        static bool LoadScene(const std::string& filePath, SceneHierarchy& hierarchy);

    private:
        static bool ProcessMaterials(ryml::ConstNodeRef materials);
        static void ProcessPostProcess(ryml::ConstNodeRef postProcessNode);
        static SceneObject* ProcessObject(ryml::ConstNodeRef objNode);
    };
}

#endif // SCENELOADER_H
