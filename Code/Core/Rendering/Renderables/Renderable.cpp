#include "Renderable.h"
#include "RenderManager.h"
#include "SceneObject.h"
#include "CCAssert.h"
#include "ShaderManager.h"
#include "GfxRenderApi.h"
#include "PipelineCache.h"

namespace CC
{
    Renderable::Renderable(Material* _material)
        : material(_material)
    {
        CC_ASSERT(material != nullptr, "Material cannot be null in Renderable constructor");
    }

    Renderable::~Renderable()
    {
    }

    void Renderable::Update()
    {
        if (IsEnabled())
        {
            AddToRenderList();
        }
    }

    Transform& Renderable::GetTransform()
    {
        CC_ASSERT(owner != nullptr, "Renderable has no owner SceneObject");
        return owner->GetTransform();
    }

    const Transform& Renderable::GetTransform() const
    {
        CC_ASSERT(owner != nullptr, "Renderable has no owner SceneObject");
        return owner->GetTransform();
    }

    void Renderable::PreRender()
    {
        CC_ASSERT(material != nullptr, "Material is not set for the renderable object.");
        CC_ASSERT(owner != nullptr, "Renderable has no owner SceneObject for PreRender");

        Mat4x4 model;
        owner->GetWorldMatrix(model);
        Gfx::RenderApi::Get()->BindPipeline(pipelineHandle);
        material->SetStandardUniforms(model);
    }

    void Renderable::AddToRenderList(void* info, int size)
    {
        CC::RenderManager::Get()->AddRenderInfo(this, info, size);
    }

    void Renderable::SetMaterial(Material* newMaterial)
    {
        CC_ASSERT(newMaterial != nullptr, "Material cannot be null in Renderable SetMaterial");
        material = newMaterial;
    }

    Gfx::PipelineHandle Renderable::CreatePipelineForMaterial(const Material* material, const Gfx::VertexLayout& vertexLayout)
    {
        return RenderManager::Get()->GetPipelineCache()->GetOrCreate(material, vertexLayout);
    }
}
