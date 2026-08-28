#ifndef TRACKERPATHS_H
#define TRACKERPATHS_H

#include <string>

namespace CC
{
    // Path resolution for the AudioTracker's user-writable storage.
    //
    // On Windows the root resolves to %PROGRAMDATA%/ChillCore/Tracker/
    // (typically C:/ProgramData/ChillCore/Tracker/). On other desktop
    // OSes a $HOME-based fallback is used so cross-platform builds stay
    // green; AudioTracker itself is a Windows-first feature, so the
    // POSIX path is mostly a placeholder until a non-Windows desktop
    // port becomes a real target.
    //
    // Both GetRootPath and the Samples/Projects subpaths ensure their
    // directory exists on first call, so callers can use the returned
    // path without further bootstrapping.
    namespace TrackerPaths
    {
        const std::string& GetRootPath();
        const std::string& GetSamplesPath();
        const std::string& GetProjectsPath();
        std::string        GetTrackerYamlPath();
    }
}

#endif // TRACKERPATHS_H
