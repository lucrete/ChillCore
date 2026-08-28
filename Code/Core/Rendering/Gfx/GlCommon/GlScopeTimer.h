#ifndef GLSCOPETIMER_H
#define GLSCOPETIMER_H

namespace CC::Gfx::GlCommon
{
    // ========================
    // GlScopeTimer
    // ========================
    //
    // Data layout for ring-buffered GPU timestamp scope timing. Shared by
    // the GL and GLES backends. The actual GL calls (glQueryCounter,
    // glGetQueryObjectui64v, glPushDebugGroup, glPopDebugGroup) stay in
    // each backend because their entry points and headers differ between
    // desktop GL 4.3 and GLES 3.1 + EXT_disjoint_timer_query. The
    // bookkeeping — frame ring, scope cap, name truncation, resolved-scope
    // output buffer — is identical in both, so the data structures live
    // here and both backends operate on a ScopeTimerState by reference.
    //
    // GL query IDs are stored as unsigned int (the underlying type of
    // GLuint on every platform); this lets the header avoid pulling in
    // GL-specific headers.

    static constexpr int FRAMES_IN_FLIGHT     = 3;
    static constexpr int MAX_SCOPES_PER_FRAME = 32;

    struct GpuScope
    {
        char         name[64]   = {};
        unsigned int queryBegin = 0;
        unsigned int queryEnd   = 0;
    };

    struct GpuScopeFrame
    {
        GpuScope scopes[MAX_SCOPES_PER_FRAME];
        int      scopeCount       = 0;
        int      currentOpenScope = -1;
        bool     everUsed         = false;
    };

    struct ResolvedScope
    {
        char  name[64]   = {};
        float durationMs = 0.0f;
    };

    struct ScopeTimerState
    {
        GpuScopeFrame frames[FRAMES_IN_FLIGHT];
        int           currentFrameIndex   = 0;
        ResolvedScope resolvedScopes[MAX_SCOPES_PER_FRAME];
        int           resolvedScopeCount  = 0;
        float         lastFrameDurationMs = 0.0f;
    };
}

#endif // GLSCOPETIMER_H
