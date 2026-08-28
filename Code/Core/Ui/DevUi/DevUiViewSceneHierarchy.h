#ifndef DEVUIVIEWSCENEHIERARCHY_H
#define DEVUIVIEWSCENEHIERARCHY_H

#include "DevUiView.h"

namespace CC
{
    class SceneObject;

    class DevUiViewSceneHierarchy : public DevUiView
    {
    public:
        DevUiViewSceneHierarchy();

        void Draw() override;
        const char* GetName() const override;

    private:
        SceneObject* selectedObject = nullptr;
        int lastSceneVersion = -1;

        void DrawSceneTree();
        void DrawObjectNode(SceneObject* object);
        void DrawPropertiesPanel();
        void DrawTransformProperties(SceneObject* object);
        void DrawComponentList(SceneObject* object);
    };
}

#endif // DEVUIVIEWSCENEHIERARCHY_H
