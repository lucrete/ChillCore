#include "RenderableQuad.h"
#include "ComponentFactory.h"
#include "MaterialManager.h"
#include "InputManager.h"
#include "PrintManager.h"
#include "ShaderManager.h"
#include "CCMath.h"
#include "GfxRenderApi.h"

namespace CC
{
    RenderableQuad::RenderableQuad(Material* material)
        : Renderable(material)
    {
        // 12-float interleaved layout: pos3 + uv2 + normal3 + tangent4
        Gfx::VertexLayout layout;
        layout.strideBytes    = 12 * sizeof(float);
        layout.attributeCount = 4;
        layout.attributes[0]  = { 0, 0                 , Gfx::VertexAttribType::Float32, 3 };
        layout.attributes[1]  = { 1, 3 * sizeof(float) , Gfx::VertexAttribType::Float32, 2 };
        layout.attributes[2]  = { 2, 5 * sizeof(float) , Gfx::VertexAttribType::Float32, 3 };
        layout.attributes[3]  = { 3, 8 * sizeof(float) , Gfx::VertexAttribType::Float32, 4 };

        pipelineHandle = CreatePipelineForMaterial(material, layout);

        Gfx::BufferDescription vbDesc;
        vbDesc.sizeBytes   = sizeof(vertices);
        vbDesc.usage       = Gfx::BufferUsage::Vertex;
        vbDesc.memory      = Gfx::BufferMemory::GpuOnly;
        vbDesc.initialData = vertices;
        vbDesc.debugName   = "RenderableQuad::Vertices";
        vertexBuffer = Gfx::RenderApi::Get()->CreateBuffer(vbDesc);

        Gfx::BufferDescription ibDesc;
        ibDesc.sizeBytes   = sizeof(indices);
        ibDesc.usage       = Gfx::BufferUsage::Index;
        ibDesc.memory      = Gfx::BufferMemory::GpuOnly;
        ibDesc.initialData = indices;
        ibDesc.debugName   = "RenderableQuad::Indices";
        indexBuffer = Gfx::RenderApi::Get()->CreateBuffer(ibDesc);
    }

    RenderableQuad::~RenderableQuad()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        if (indexBuffer.IsValid())  { gfxApi->DestroyBuffer(indexBuffer); }
        if (vertexBuffer.IsValid()) { gfxApi->DestroyBuffer(vertexBuffer); }
        // Pipeline lifetime is owned by PipelineCache.
    }

    int RenderableQuad::Render(void* renderInfo)
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        gfxApi->BindVertexBuffer(0, vertexBuffer, 0, 12 * sizeof(float));
        gfxApi->BindIndexBuffer(indexBuffer, Gfx::IndexType::Uint32);
        gfxApi->DrawIndexed(6, 1, 0, 0);
        return 0;
    }

    void RenderableQuad::AddToRenderList()
    {
        Renderable::AddToRenderList(0, 0);
    }
}

// ========================
// Factory Registration
// ========================

static CC::Component* CreateRenderableQuad(ryml::ConstNodeRef componentData)
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
    return new CC::RenderableQuad(material);
}

static CC::ComponentRegistrar renderableQuadRegistrar("RenderableQuad", CreateRenderableQuad);
