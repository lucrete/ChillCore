#ifndef SAMPLELIBRARY_H
#define SAMPLELIBRARY_H

#include <string>
#include <vector>

namespace CC
{
    // ========================
    // Sample status
    // ========================

    // The four states a sample entry can be in after LoadAndVerify
    // walks Tracker.yaml against the on-disk Samples/ directory.
    enum class SampleStatus
    {
        Ok,         // YAML entry matches the on-disk file (size + sha1)
        Broken,     // YAML entry references a missing file
        Modified,   // file exists but size or sha1 differs from the YAML record
        Orphan      // file exists in Samples/ but no YAML entry references it
    };

    // ========================
    // Sample entry
    // ========================

    // One row in Tracker.yaml's samples list, paired with its current
    // verification state. The on-disk file is always at
    // TrackerPaths::GetSamplesPath() / fileName; absolute paths are
    // not stored so the library survives moves of the user's storage
    // root.
    struct SampleEntry
    {
        std::string  id;                            // stable across project files
        std::string  fileName;                      // basename inside Samples/
        size_t       sizeBytes      = 0;
        std::string  sha1;                          // 40-char lowercase hex
        SampleStatus status         = SampleStatus::Ok;
    };

    // ========================
    // Sample library
    // ========================

    // Reads, writes, and verifies %PROGRAMDATA%/ChillCore/Tracker/Tracker.yaml.
    // Sole owner of the Tracker.yaml round-trip; consumers (the sample
    // panel UI, project loader, audition path) read entries via
    // GetEntries.
    //
    // First-cut scope (Phase 2 step 1): YAML round-trip + Broken/Modified
    // verification against on-disk files. Orphan detection (files in
    // Samples/ with no YAML entry) is deferred until directory listing
    // is available through PlatformFileSystem; the SampleStatus::Orphan
    // value is in the enum already so the verification UI can ship with
    // it disabled and enable it later.
    class SampleLibrary
    {
    public:
        SampleLibrary();
        virtual ~SampleLibrary();

        static SampleLibrary* Get();

        // Reads Tracker.yaml (creating it if missing), verifies each
        // entry against on-disk files, and populates entries. Returns
        // true on success even when individual entries are broken or
        // modified — the verification result is on each entry.
        bool LoadAndVerify();

        // Atomically rewrites Tracker.yaml with the current entries.
        // Orphan-status entries are skipped on save; they exist only
        // in-memory until the user dispositions them (register or
        // delete from the panel UI).
        bool Save() const;

        // Imports a file from outside the library. Copies the bytes
        // into Samples/ under a deduplicated name, computes SHA1,
        // appends an entry, and rewrites Tracker.yaml atomically. The
        // generated id is returned via outNewId on success.
        bool Import(const std::string& sourcePath, std::string& outNewId);

        // Removes the entry with the given id from the in-memory list
        // and rewrites Tracker.yaml. The on-disk file in Samples/ is
        // left in place — subsequent loads will re-discover it as an
        // orphan. Used as the disposition action for Broken / Modified
        // entries surfaced by verification.
        bool Forget(const std::string& id);

        // Promotes an existing-on-disk file (typically an orphan
        // detected by AppendOrphans) to a proper library entry: hashes
        // the file, generates a stable id from its name, and saves
        // Tracker.yaml. The orphan placeholder for that filename is
        // dropped from the in-memory list.
        bool RegisterOrphan(const std::string& fileName, std::string& outNewId);

        const std::vector<SampleEntry>& GetEntries() const { return entries; }

        // Monotonically increasing on every mutation. UI controllers
        // poll this to trigger refresh when entries change in ways that
        // size() alone wouldn't catch (e.g. status flips).
        int GetVersion() const { return version; }

    private:
        static SampleLibrary* instance;

        bool ReadAndParseTrackerYaml(std::string& outYamlText, bool& outDidExist) const;
        bool ParseSampleEntries(const std::string& yamlText, std::vector<SampleEntry>& outEntries) const;
        void VerifyEntries(std::vector<SampleEntry>& entriesToVerify) const;
        void AppendOrphans(std::vector<SampleEntry>& entriesToAugment) const;
        std::string EmitYaml() const;

        // Generates a stable id from a candidate filename, deduplicated
        // against existing entries. The id mirrors the basename minus
        // the extension, with non-alphanumerics collapsed to '_'.
        // Sample files retain their original on-disk filenames; only
        // the id needs deduplication.
        std::string GenerateUniqueId(const std::string& sourceFileName) const;

        std::vector<SampleEntry> entries;
        int                      version = 0;
    };
}

#endif // SAMPLELIBRARY_H
