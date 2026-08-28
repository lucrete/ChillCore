#ifndef TRACKERPROJECTFILE_H
#define TRACKERPROJECTFILE_H

#include <string>

namespace CC
{
    struct TrackerProject;

    // Save and load .cctrack tracker project files. The format is YAML
    // mirroring TrackerProject. Sample references are by id (resolved
    // against Tracker.yaml on load); sample library state is not
    // embedded.
    //
    // Tracker-scoped name so a future "project file" concept for an
    // unrelated feature (asset project, scene project, ...) does not
    // collide.
    namespace TrackerProjectFile
    {
        static const int TRACKER_PROJECT_FILE_VERSION = 1;

        // Returns the canonical "default" project path under the
        // tracker's storage root. Used by the v1 single-file save/load
        // until the in-engine browser lands.
        std::string GetDefaultProjectPath();

        // Writes the project to filePath via atomic temp+rename. Old
        // file is preserved on any failure.
        bool Save(const TrackerProject& project, const std::string& filePath);

        // Replaces project's contents with the parsed file. Returns
        // false on parse error or version mismatch; project state is
        // left unchanged in that case. Caller is expected to bump
        // project.version and clear TrackerCommandHistory after a
        // successful load.
        bool Load(const std::string& filePath, TrackerProject& outProject);
    }
}

#endif // TRACKERPROJECTFILE_H
