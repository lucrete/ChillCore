#include "RenderableMesh.h"
#include "CCFile.h"
#include "ComponentFactory.h"
#include "MaterialManager.h"
#include "RenderManager.h"
#include "ObjLoader.h"
#include "GltfLoader.h"
#include "PrintManager.h"
#include "GfxRenderApi.h"

namespace CC
{
    RenderableMesh::RenderableMesh(Material* material)
        : Renderable(material), m_IndexCount(0), vertexStride(0)
    {
    }

    RenderableMesh::~RenderableMesh()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        if (indexBuffer.IsValid())  { gfxApi->DestroyBuffer(indexBuffer); }
        if (vertexBuffer.IsValid()) { gfxApi->DestroyBuffer(vertexBuffer); }
        // Pipeline lifetime is owned by PipelineCache.
    }

    void RenderableMesh::Initialize(const float* vertices, const unsigned int* indices, int verticesSize, int indicesSize)
    {
        // 8-float interleaved layout: pos3 + uv2 + normal3
        Gfx::VertexLayout layout;
        layout.strideBytes    = 8 * sizeof(float);
        layout.attributeCount = 3;
        layout.attributes[0]  = { 0, 0                 , Gfx::VertexAttribType::Float32, 3 };
        layout.attributes[1]  = { 1, 3 * sizeof(float) , Gfx::VertexAttribType::Float32, 2 };
        layout.attributes[2]  = { 2, 5 * sizeof(float) , Gfx::VertexAttribType::Float32, 3 };
        vertexStride = layout.strideBytes;

        pipelineHandle = CreatePipelineForMaterial(material, layout);

        Gfx::BufferDescription vbDesc;
        vbDesc.sizeBytes   = verticesSize;
        vbDesc.usage       = Gfx::BufferUsage::Vertex;
        vbDesc.memory      = Gfx::BufferMemory::GpuOnly;
        vbDesc.initialData = vertices;
        vbDesc.debugName   = "RenderableMesh::Vertices";
        vertexBuffer = Gfx::RenderApi::Get()->CreateBuffer(vbDesc);

        Gfx::BufferDescription ibDesc;
        ibDesc.sizeBytes   = indicesSize;
        ibDesc.usage       = Gfx::BufferUsage::Index;
        ibDesc.memory      = Gfx::BufferMemory::GpuOnly;
        ibDesc.initialData = indices;
        ibDesc.debugName   = "RenderableMesh::Indices";
        indexBuffer = Gfx::RenderApi::Get()->CreateBuffer(ibDesc);
    }

    void RenderableMesh::InitializeWithTangents(const float* vertices, const unsigned int* indices, int verticesSize, int indicesSize)
    {
        // 12-float interleaved layout: pos3 + uv2 + normal3 + tangent4
        Gfx::VertexLayout layout;
        layout.strideBytes    = 12 * sizeof(float);
        layout.attributeCount = 4;
        layout.attributes[0]  = { 0, 0                 , Gfx::VertexAttribType::Float32, 3 };
        layout.attributes[1]  = { 1, 3 * sizeof(float) , Gfx::VertexAttribType::Float32, 2 };
        layout.attributes[2]  = { 2, 5 * sizeof(float) , Gfx::VertexAttribType::Float32, 3 };
        layout.attributes[3]  = { 3, 8 * sizeof(float) , Gfx::VertexAttribType::Float32, 4 };
        vertexStride = layout.strideBytes;

        pipelineHandle = CreatePipelineForMaterial(material, layout);

        Gfx::BufferDescription vbDesc;
        vbDesc.sizeBytes   = verticesSize;
        vbDesc.usage       = Gfx::BufferUsage::Vertex;
        vbDesc.memory      = Gfx::BufferMemory::GpuOnly;
        vbDesc.initialData = vertices;
        vbDesc.debugName   = "RenderableMesh::VerticesTangent";
        vertexBuffer = Gfx::RenderApi::Get()->CreateBuffer(vbDesc);

        Gfx::BufferDescription ibDesc;
        ibDesc.sizeBytes   = indicesSize;
        ibDesc.usage       = Gfx::BufferUsage::Index;
        ibDesc.memory      = Gfx::BufferMemory::GpuOnly;
        ibDesc.initialData = indices;
        ibDesc.debugName   = "RenderableMesh::IndicesTangent";
        indexBuffer = Gfx::RenderApi::Get()->CreateBuffer(ibDesc);
    }

    int RenderableMesh::Render(void* renderInfo)
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        gfxApi->BindVertexBuffer(0, vertexBuffer, 0, vertexStride);
        gfxApi->BindIndexBuffer(indexBuffer, Gfx::IndexType::Uint32);
        gfxApi->DrawIndexed(m_IndexCount, 1, 0, 0);
        return 0;
    }

    void RenderableMesh::AddToRenderList()
    {
        Renderable::AddToRenderList(0, 0);
    }
}

// ========================
// Factory Registration
// ========================

static bool HasGltfExtension(const std::string& path)
{
    std::string ext = CCFile::GetExtension(path);
    return ext == ".gltf" || ext == ".glb" || ext == ".GLTF" || ext == ".GLB";
}

static CC::Component* CreateRenderableMesh(ryml::ConstNodeRef componentData)
{
    // Check if mesh property is specified
    if (componentData.has_child("mesh"))
    {
        std::string meshPath = NodeToString(componentData["mesh"]);

        // Check file extension to determine loader
        if (HasGltfExtension(meshPath))
        {
            CC::RenderableMesh* mesh = CC::GltfLoader::LoadGltfMesh(meshPath);
            if (mesh == nullptr)
            {
                CCPrint(CC::PrintManager::CHANNEL_WARN, "RenderableMesh: Failed to load glTF mesh '%s'", meshPath.c_str());
            }
            return mesh;
        }
        else
        {
            // Use ObjLoader for .obj files (default)
            // Check if a material override is specified
            if (componentData.has_child("material"))
            {
                std::string matName = NodeToString(componentData["material"]);
                CC::Material* material = CC::MaterialManager::Get()->GetMaterial(matName);
                CC::RenderableMesh* mesh = CC::ObjLoader::LoadObj(meshPath, material);
                if (mesh == nullptr)
                {
                    CCPrint(CC::PrintManager::CHANNEL_WARN, "RenderableMesh: Failed to load mesh '%s'", meshPath.c_str());
                }
                return mesh;
            }
            else
            {
                // Load mesh with material from MTL file
                CC::RenderableMesh* mesh = CC::ObjLoader::LoadObj(meshPath);
                if (mesh == nullptr)
                {
                    CCPrint(CC::PrintManager::CHANNEL_WARN, "RenderableMesh: Failed to load mesh '%s'", meshPath.c_str());
                }
                return mesh;
            }
        }
    }

    // Fallback to material-only creation (original behavior)
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
    return new CC::RenderableMesh(material);
}

static CC::ComponentRegistrar renderableMeshRegistrar("RenderableMesh", CreateRenderableMesh);
