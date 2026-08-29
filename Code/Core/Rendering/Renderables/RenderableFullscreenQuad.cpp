#include "RenderManager.h"
#include "RenderableFullscreenQuad.h"
#include "ShaderManager.h"
#include "InputManager.h"
#include "PrintManager.h"
#include "CCMat4x4.h"
#include "GfxRenderApi.h"
#include "QuadMesh.h"

namespace CC
{
    static Gfx::VertexLayout MakeFullscreenQuadLayout()
    {
        Gfx::VertexLayout layout;
        layout.strideBytes    = QUAD_STRIDE_BYTES;
        layout.attributeCount = 2;
        layout.attributes[0]  = { 0, 0                 , Gfx::VertexAttribType::Float32, 2 };
        layout.attributes[1]  = { 1, 2 * sizeof(float) , Gfx::VertexAttribType::Float32, 2 };
        return layout;
    }

    RenderableFullscreenQuad::RenderableFullscreenQuad(Material* material)
        : Renderable(material)
    {
        Gfx::BufferDescription vbDesc;
        vbDesc.sizeBytes   = sizeof(QUAD_VERTICES);
        vbDesc.usage       = Gfx::BufferUsage::Vertex;
        vbDesc.memory      = Gfx::BufferMemory::GpuOnly;
        vbDesc.initialData = QUAD_VERTICES;
        vbDesc.debugName   = "RenderableFullscreenQuad::Vertices";
        vertexBuffer = Gfx::RenderApi::Get()->CreateBuffer(vbDesc);

        // SetMaterial builds the pipeline. Must be called before Render.
        SetMaterial(material);
        SetOnRenderCallback(nullptr);
    }

    RenderableFullscreenQuad::~RenderableFullscreenQuad()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        if (vertexBuffer.IsValid())
        {
            gfxApi->DestroyBuffer(vertexBuffer);
        }
        // Pipeline lifetime is owned by PipelineCache.
        SetOnRenderCallback(nullptr);
    }

    void RenderableFullscreenQuad::SetMaterial(Material* newMaterial)
    {
        Renderable::SetMaterial(newMaterial);

        // Pipeline bakes shader + blend/depth state from the material. When
        // the material changes (e.g. ProceduralArtController swaps effects),
        // the cache returns the pipeline keyed on the new (material, layout);
        // the old one stays resident in the cache for any future user.
        pipelineHandle = CreatePipelineForMaterial(material, MakeFullscreenQuadLayout());

        // Defaults for the MaterialParams members the procedural-art shaders
        // declare. A shader that declares none of them stores the values and
        // ignores them.
        material->SetUniform("aspectRatio", 1.0f);
        material->SetUniform("center", Vector2(0, 0));
        material->SetUniform("scale", 1.0f);
    }

    void RenderableFullscreenQuad::PreRender()
    {
        Mat4x4 identity;
        identity.Identity();
        Gfx::RenderApi::Get()->BindPipeline(pipelineHandle);
        material->SetStandardUniforms(identity);
    }

    int RenderableFullscreenQuad::Render(void* renderInfo)
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        int width = 0;
        int height = 0;
        RenderManager::Get()->GetWindowSize(width, height);
        float ratio = width / (float)height;
        material->SetUniform("aspectRatio", ratio);

        if (onRenderCallback)
        {
            onRenderCallback(*material);
        }

        gfxApi->BindVertexBuffer(0, vertexBuffer, 0, QUAD_STRIDE_BYTES);
        gfxApi->Draw(QUAD_VERTEX_COUNT, 1, 0);

        return 0;
    }

    void RenderableFullscreenQuad::AddToRenderList()
    {
        Renderable::AddToRenderList(0, 0);
    }
}
