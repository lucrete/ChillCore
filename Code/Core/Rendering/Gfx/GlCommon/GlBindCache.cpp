#include "GlBindCache.h"

namespace CC::Gfx::GlCommon
{
    void BindCacheState::Reset()
    {
        for (int i = 0; i < MAX_BOUND_TEXTURES; i++)
        {
            textureUnits[i] = TextureUnitBinding();
        }
        for (int i = 0; i < MAX_BOUND_UNIFORM_BUFFERS; i++)
        {
            uniformBuffers[i] = UniformBufferBinding();
        }
        ResetVertexBuffers();

        vertexArrayObject = 0;
        activeTextureUnit = -1;
    }

    void BindCacheState::ResetVertexBuffers()
    {
        for (int i = 0; i < MAX_BOUND_VERTEX_BUFFERS; i++)
        {
            vertexBuffers[i] = VertexBufferBinding();
        }
    }
}
