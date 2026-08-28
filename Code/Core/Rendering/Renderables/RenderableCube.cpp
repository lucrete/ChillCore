#include "RenderableCube.h"
#include "ComponentFactory.h"
#include "MaterialManager.h"
#include "InputManager.h"
#include "PrintManager.h"
#include "ShaderManager.h"
#include "CCMath.h"

namespace CC
{
    RenderableCube::RenderableCube(Material* material)
        : RenderableMesh(material)
    {
        // Initialize the mesh with the vertices and indices from the header
        Initialize(vertices, indices, sizeof(vertices), sizeof(indices));
        m_IndexCount = 36; // 12 triangles with 3 vertices each
    }

    RenderableCube::~RenderableCube()
    {
        // Base class destructor will handle cleanup
    }

    void RenderableCube::AddToRenderList()
    {
        Renderable::AddToRenderList(0, 0);
    }
}

// ========================
// Factory Registration
// ========================

static CC::Component* CreateRenderableCube(ryml::ConstNodeRef componentData)
{
    CC::Material* material = nullptr;
    if (componentData.has_child("material"))
    {
        std::string matName = NodeToString(componentData["material"]);
        material = CC::MaterialManager::Get()->GetMaterial(matName);
    }

    if (material == nullptr)
    {
        return nullptr;
    }
    return new CC::RenderableCube(material);
}

static CC::ComponentRegistrar renderableCubeRegistrar("RenderableCube", CreateRenderableCube);
