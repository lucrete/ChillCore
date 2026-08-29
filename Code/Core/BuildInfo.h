#ifndef BUILDINFO_H
#define BUILDINFO_H

namespace CC
{
    // Identifies the build the running binary came from. Values are compiled
    // in by the build pipeline. A build the pipeline did not produce — an
    // interactive build from the IDE — reports itself as unlabelled, because
    // it cannot be reproduced from a record.
    namespace BuildInfo
    {
        const char* GetBuildId();
        const char* GetCommitHash();
        const char* GetConfiguration();
        const char* GetTimestamp();
        bool IsLabelled();
    }
}

#endif // BUILDINFO_H
