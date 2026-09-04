#ifndef GLBINDCACHE_H
#define GLBINDCACHE_H

#include "GfxDescriptions.h"

namespace CC::Gfx::GlCommon
{
    // ========================
    // GlBindCache
    // ========================
    //
    // Records what the backend last bound at each slot so a repeated bind
    // can be skipped. Shared by the GL and GLES backends: the bookkeeping
    // is identical in both, only the GL entry points that act on it differ.
    //
    // The cache describes what the backend itself bound. Any code path that
    // reaches the driver directly — a raw framebuffer bind, a texture upload,
    // a third-party overlay — leaves it describing state that is no longer
    // there, and must call Reset before the next tracked bind. A stale entry
    // does not fail loudly; it shows up as a wrong-texture or black frame.
    //
    // GL object names are stored as unsigned int (the underlying type of
    // GLuint everywhere) so the header pulls in no GL headers.

    // Stands for "nothing is recorded here" where zero is a binding a caller
    // can legitimately ask for.
    static constexpr unsigned int UNKNOWN_GL_NAME = 0xFFFFFFFFu;

    struct TextureUnitBinding
    {
        unsigned int target  = 0;
        // Texture name 0 means the unit's contents are unknown; a bind
        // always names a live texture, so zero can never be a cached value.
        unsigned int texture = 0;
        // Sampler 0 is a real binding — it hands sampling back to the
        // texture's own parameters — so unknown needs its own value.
        unsigned int sampler = UNKNOWN_GL_NAME;
    };

    struct VertexBufferBinding
    {
        unsigned int buffer      = 0;
        int          offsetBytes = 0;
        int          strideBytes = 0;
    };

    struct UniformBufferBinding
    {
        unsigned int buffer      = 0;
        int          offsetBytes = 0;
        int          sizeBytes   = 0;
    };

    struct BindCacheState
    {
        TextureUnitBinding   textureUnits[MAX_BOUND_TEXTURES];
        VertexBufferBinding  vertexBuffers[MAX_BOUND_VERTEX_BUFFERS];
        UniformBufferBinding uniformBuffers[MAX_BOUND_UNIFORM_BUFFERS];

        // The vertex array object the recorded vertex-buffer bindings belong
        // to. Vertex buffer bindings are stored in the vertex array object
        // rather than in the context, so they survive only as long as that
        // object stays bound.
        unsigned int vertexArrayObject = 0;

        // Texture unit glActiveTexture last selected. -1 means unknown.
        int activeTextureUnit = -1;

        void Reset();
        void ResetVertexBuffers();
    };
}

#endif // GLBINDCACHE_H
