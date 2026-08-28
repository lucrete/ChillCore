#include "PlatformFileSystem.h"
#include "CCAssert.h"

namespace CC
{
    PlatformFileSystem* PlatformFileSystem::instance = nullptr;

    PlatformFileSystem::PlatformFileSystem()
    {
        CC_ASSERT(instance == nullptr, "PlatformFileSystem already created");
        instance = this;
    }

    PlatformFileSystem::~PlatformFileSystem()
    {
        instance = nullptr;
    }

    PlatformFileSystem* PlatformFileSystem::Get()
    {
        CC_ASSERT(instance != nullptr, "PlatformFileSystem not created yet");
        return instance;
    }
}
