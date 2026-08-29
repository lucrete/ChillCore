#include "BuildInfo.h"

#include <cstring>

// BuildInfoGenerated.h is written by Tools/Build/Build.py and is not checked
// in. Its absence is the normal case for an interactive build, so the
// fallbacks below have to produce something honest rather than fail.
#if defined(__has_include)
    #if __has_include("BuildInfoGenerated.h")
        #include "BuildInfoGenerated.h"
    #endif
#endif

#ifndef CC_BUILD_ID
    #define CC_BUILD_ID "Unlabelled"
#endif

#ifndef CC_BUILD_COMMIT_HASH
    #define CC_BUILD_COMMIT_HASH "Unknown"
#endif

#ifndef CC_BUILD_CONFIGURATION
    #ifdef CC_DEBUG
        #define CC_BUILD_CONFIGURATION "Debug"
    #else
        #define CC_BUILD_CONFIGURATION "Release"
    #endif
#endif

#ifndef CC_BUILD_TIMESTAMP
    #define CC_BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

namespace CC
{
    namespace BuildInfo
    {
        static const char* BUILD_ID = CC_BUILD_ID;
        static const char* COMMIT_HASH = CC_BUILD_COMMIT_HASH;
        static const char* CONFIGURATION = CC_BUILD_CONFIGURATION;
        static const char* TIMESTAMP = CC_BUILD_TIMESTAMP;
        static const char* UNLABELLED_ID = "Unlabelled";

        const char* GetBuildId()
        {
            return BUILD_ID;
        }

        const char* GetCommitHash()
        {
            return COMMIT_HASH;
        }

        const char* GetConfiguration()
        {
            return CONFIGURATION;
        }

        const char* GetTimestamp()
        {
            return TIMESTAMP;
        }

        bool IsLabelled()
        {
            return std::strcmp(BUILD_ID, UNLABELLED_ID) != 0;
        }
    }
}
