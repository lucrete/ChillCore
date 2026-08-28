#ifndef RENDERABLESPHERE_H
#define RENDERABLESPHERE_H

#include "Renderable.h"

namespace CC
{
    class RenderableMesh;

    class RenderableSphere : public Renderable
    {
    public:
        static void InitSharedMesh();
        static void ShutdownSharedMesh();

        RenderableSphere(Material* material);
        virtual ~RenderableSphere();

        virtual const char* GetTypeName() const override { return "RenderableSphere"; }

        virtual int Render(void* renderInfo) override;
        virtual void AddToRenderList() override;

    private:
        static RenderableMesh* sharedMesh;
    };
}

#endif // RENDERABLESPHERE_H
