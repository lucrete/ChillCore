#ifndef GLHANDLEPOOL_H
#define GLHANDLEPOOL_H

#include <vector>
#include <cstdint>

namespace CC::Gfx::GlCommon
{
    // ========================
    // GlHandlePool
    // ========================
    //
    // Free-list slot allocator shared by the GL and GLES backends. Both
    // backends use the same handle layout (id = poolIndex + 1, id 0
    // reserved as invalid) and the same recycle-via-free-list strategy.
    // The actual pools live in each backend (vector<GlBuffer>, etc.);
    // this helper only manages slot indices.

    uint32_t AcquireSlot(std::vector<uint32_t>& freeList, uint32_t poolSize);
}

#endif // GLHANDLEPOOL_H
