#include "RenderableSphere.h"
#include "ComponentFactory.h"
#include "RenderableMesh.h"
#include "ObjLoader.h"
#include "MaterialManager.h"
#include "PrintManager.h"
#include "CCAssert.h"
#include "GfxRenderApi.h"

namespace CC
{
    RenderableMesh* RenderableSphere::sharedMesh = nullptr;

    void RenderableSphere::InitSharedMesh()
    {
        if (sharedMesh != nullptr)
        {
            return;
        }

        Material* defaultMaterial = MaterialManager::Get()->GetMaterial("DefaultBasic");
        sharedMesh = ObjLoader::LoadObj("Data\\Meshes\\Primitives\\sphere.obj", defaultMaterial);

        if (sharedMesh != nullptr)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "RenderableSphere: Loaded shared mesh");
        }
        else
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "RenderableSphere: Failed to load shared mesh");
        }
    }

    void RenderableSphere::ShutdownSharedMesh()
    {
        delete sharedMesh;
        sharedMesh = nullptr;
    }

    RenderableSphere::RenderableSphere(Material* material)
        : Renderable(material)
    {
        CC_ASSERT(sharedMesh != nullptr, "RenderableSphere::InitSharedMesh() must be called before creating instances");

        // Shared mesh is loaded via ObjLoader as 8-float pos3+uv2+normal3.
        // Each instance owns a pipeline built from its own material so the
        // shader + blend/depth state track the instance, not the shared mesh.
        Gfx::VertexLayout layout;
        layout.strideBytes    = 8 * sizeof(float);
        layout.attributeCount = 3;
        layout.attributes[0]  = { 0, 0                 , Gfx::VertexAttribType::Float32, 3 };
        layout.attributes[1]  = { 1, 3 * sizeof(float) , Gfx::VertexAttribType::Float32, 2 };
        layout.attributes[2]  = { 2, 5 * sizeof(float) , Gfx::VertexAttribType::Float32, 3 };

        pipelineHandle = CreatePipelineForMaterial(material, layout);
    }

    RenderableSphere::~RenderableSphere()
    {
        // Pipeline lifetime is owned by PipelineCache; nothing to destroy here.
    }

    int RenderableSphere::Render(void* renderInfo)
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        gfxApi->BindVertexBuffer(0, sharedMesh->GetVertexBuffer(), 0, sharedMesh->GetVertexStride());
        gfxApi->BindIndexBuffer(sharedMesh->GetIndexBuffer(), Gfx::IndexType::Uint32);
        gfxApi->DrawIndexed(sharedMesh->GetIndexCount(), 1, 0, 0);
        return 0;
    }

    void RenderableSphere::AddToRenderList()
    {
        Renderable::AddToRenderList(0, 0);
    }
}

// ========================
// Factory Registration
// ========================

static CC::Component* CreateRenderableSphere(ryml::ConstNodeRef componentData)
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
    return new CC::RenderableSphere(material);
}

static CC::ComponentRegistrar renderableSphereRegistrar("RenderableSphere", CreateRenderableSphere);
