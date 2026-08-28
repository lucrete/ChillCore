#include "GfxRenderApi.h"
#include "CCAssert.h"

namespace CC::Gfx
{
    RenderApi* RenderApi::instance = nullptr;

    RenderApi::RenderApi()
    {
        CC_ASSERT(instance == nullptr, "Gfx::RenderApi already created");
        instance = this;
    }

    RenderApi::~RenderApi()
    {
        instance = nullptr;
    }

    RenderApi* RenderApi::Get()
    {
        CC_ASSERT(instance != nullptr, "Gfx::RenderApi not created yet");
        return instance;
    }
}
