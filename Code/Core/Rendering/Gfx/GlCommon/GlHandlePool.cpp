#include "GlHandlePool.h"

namespace CC::Gfx::GlCommon
{
    uint32_t AcquireSlot(std::vector<uint32_t>& freeList, uint32_t poolSize)
    {
        uint32_t slotIndex = 0;
        if (freeList.empty())
        {
            slotIndex = poolSize;
        }
        else
        {
            slotIndex = freeList.back();
            freeList.pop_back();
        }
        return slotIndex;
    }
}
