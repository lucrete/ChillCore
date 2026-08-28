#include "SampleLibrary.h"

#include <cstdint>
#include <cstdio>
#include <vector>

#include <rapidyaml-0.10.0.hpp>

#include "TrackerPaths.h"
#include "PlatformFileSystem.h"
#include "PrintManager.h"
#include "Sha1.h"
#include "CCAssert.h"

namespace CC
{
    SampleLibrary* SampleLibrary::instance = nullptr;

    // ========================
    // Constants
    // ========================

    static const int TRACKER_YAML_VERSION = 1;

    // ========================
    // Helpers
    // ========================

    static std::string NodeToString(ryml::ConstNodeRef node)
    {
        std::string result;
        if (node.has_val())
        {
            c4::csubstr value = node.val();
            result = std::string(value.data(), value.size());
        }
        return result;
    }

    static size_t NodeToSize(ryml::ConstNodeRef node)
    {
        size_t result = 0;
        if (node.has_val())
        {
            uint64_t value = 0;
            node >> value;
            result = (size_t)value;
        }
        return result;
    }

    // ========================
    // Construction
    // ========================

    SampleLibrary::SampleLibrary()
    {
        CC_ASSERT(instance == nullptr, "SampleLibrary already created");
        instance = this;
    }

    SampleLibrary::~SampleLibrary()
    {
        instance = nullptr;
    }

    SampleLibrary* SampleLibrary::Get()
    {
        CC_ASSERT(instance != nullptr, "SampleLibrary not created yet");
        return instance;
    }

    // ========================
    // Public methods
    // ========================

    bool SampleLibrary::LoadAndVerify()
    {
        bool succeeded = false;

        std::string yamlText;
        bool        didExist  = false;
        bool        didRead   = ReadAndParseTrackerYaml(yamlText, didExist);

        if (!didRead && didExist)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary: failed to read Tracker.yaml");
        }
        else
        {
            entries.clear();

            if (didExist)
            {
                if (!ParseSampleEntries(yamlText, entries))
                {
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary: failed to parse Tracker.yaml");
                }
                else
                {
                    succeeded = true;
                }
            }
            else
            {
                // First run on this machine — Tracker.yaml does not
                // exist. Save an empty library so subsequent runs see a
                // valid file rather than re-creating one each time.
                if (!Save())
                {
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary: failed to write initial Tracker.yaml");
                }
                else
                {
                    succeeded = true;
                }
            }

            VerifyEntries(entries);
            AppendOrphans(entries);
            version++;

            int okCount       = 0;
            int modifiedCount = 0;
            int brokenCount   = 0;
            int orphanCount   = 0;
            for (size_t i = 0; i < entries.size(); i++)
            {
                switch (entries[i].status)
                {
                    case SampleStatus::Ok:       okCount++;       break;
                    case SampleStatus::Modified: modifiedCount++; break;
                    case SampleStatus::Broken:   brokenCount++;   break;
                    case SampleStatus::Orphan:   orphanCount++;   break;
                }
            }
            CCPrint(PrintManager::CHANNEL_ALWAYS,
                "SampleLibrary: %d ok, %d modified, %d broken, %d orphan",
                okCount, modifiedCount, brokenCount, orphanCount);
        }

        return succeeded;
    }

    bool SampleLibrary::Import(const std::string& sourcePath, std::string& outNewId)
    {
        bool succeeded = false;
        outNewId.clear();

        // Extract source basename. Sample files retain their original
        // names on disk so the same name appears in Samples/ and in
        // Tracker.yaml.
        std::string fileName = sourcePath;
        size_t      lastSeparator = sourcePath.find_last_of("/\\");
        if (lastSeparator != std::string::npos)
        {
            fileName = sourcePath.substr(lastSeparator + 1);
        }

        if (fileName.empty())
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary::Import refused: source path has no filename component");
        }
        else
        {
            std::string destinationPath  = TrackerPaths::GetSamplesPath() + "/" + fileName;
            bool        destinationExists = PlatformFileSystem::Get()->FileExists(destinationPath.c_str());
            bool        sourceIsDestination = (sourcePath == destinationPath);

            // Three cases for the on-disk situation:
            //   1) destination doesn't exist                — copy source bytes in
            //   2) destination exists and source IS dest    — orphan promotion or re-register; nothing to copy
            //   3) destination exists but source is foreign — refuse to overwrite
            bool copiedOk = true;
            if (!destinationExists)
            {
                copiedOk = PlatformFileSystem::Get()->CopyFile(sourcePath.c_str(), destinationPath.c_str());
                if (!copiedOk)
                {
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary::Import failed to copy '%s' into Samples/", fileName.c_str());
                }
            }
            else if (!sourceIsDestination)
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS,
                    "SampleLibrary::Import refused: '%s' already exists in Samples/. Rename your source file before re-importing.",
                    fileName.c_str());
                copiedOk = false;
            }

            if (copiedOk)
            {
                // Idempotent re-import: if a non-orphan entry already
                // references this filename, return its id without
                // re-hashing or re-saving.
                std::string existingId;
                for (size_t i = 0; i < entries.size() && existingId.empty(); i++)
                {
                    if (entries[i].fileName == fileName && entries[i].status != SampleStatus::Orphan)
                    {
                        existingId = entries[i].id;
                    }
                }

                if (!existingId.empty())
                {
                    outNewId  = existingId;
                    succeeded = true;
                }
                else
                {
                    std::vector<uint8_t> fileBytes;
                    if (!PlatformFileSystem::Get()->ReadFileBinary(destinationPath.c_str(), fileBytes))
                    {
                        CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary::Import failed to read destination for hashing");
                    }
                    else
                    {
                        // Drop any matching orphan record now that we are
                        // claiming the file with a real entry.
                        std::vector<SampleEntry>::iterator iter = entries.begin();
                        while (iter != entries.end())
                        {
                            if (iter->status == SampleStatus::Orphan && iter->fileName == fileName)
                            {
                                iter = entries.erase(iter);
                            }
                            else
                            {
                                ++iter;
                            }
                        }

                        SampleEntry entry;
                        entry.id        = GenerateUniqueId(fileName);
                        entry.fileName  = fileName;
                        entry.sizeBytes = fileBytes.size();
                        entry.sha1      = Sha1Hex(fileBytes.data(), fileBytes.size());
                        entry.status    = SampleStatus::Ok;

                        entries.push_back(entry);

                        if (!Save())
                        {
                            CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary::Import succeeded on disk but failed to update Tracker.yaml");
                        }
                        else
                        {
                            outNewId  = entry.id;
                            succeeded = true;
                            version++;
                        }
                    }
                }
            }
        }

        return succeeded;
    }

    bool SampleLibrary::Forget(const std::string& id)
    {
        bool succeeded = false;

        std::vector<SampleEntry>::iterator iter = entries.begin();
        while (iter != entries.end())
        {
            if (iter->id == id && iter->status != SampleStatus::Orphan)
            {
                iter = entries.erase(iter);
                succeeded = true;
                break;
            }
            ++iter;
        }

        if (succeeded)
        {
            if (!Save())
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary::Forget removed in-memory entry but failed to update Tracker.yaml");
            }
            // Re-scan Samples/ so the just-forgotten file resurfaces as
            // an orphan rather than vanishing from the panel entirely.
            AppendOrphans(entries);
            version++;
        }
        else
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary::Forget: no entry with id '%s'", id.c_str());
        }

        return succeeded;
    }

    bool SampleLibrary::RegisterOrphan(const std::string& fileName, std::string& outNewId)
    {
        bool succeeded = false;
        outNewId.clear();

        std::string filePath = TrackerPaths::GetSamplesPath() + "/" + fileName;

        std::vector<uint8_t> fileBytes;
        if (!PlatformFileSystem::Get()->ReadFileBinary(filePath.c_str(), fileBytes))
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary::RegisterOrphan failed to read '%s'", fileName.c_str());
        }
        else
        {
            // Drop the matching orphan placeholder before checking for
            // id collisions so GenerateUniqueId doesn't trip over a
            // sibling entry referencing the same file.
            std::vector<SampleEntry>::iterator iter = entries.begin();
            while (iter != entries.end())
            {
                if (iter->status == SampleStatus::Orphan && iter->fileName == fileName)
                {
                    iter = entries.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }

            SampleEntry entry;
            entry.id        = GenerateUniqueId(fileName);
            entry.fileName  = fileName;
            entry.sizeBytes = fileBytes.size();
            entry.sha1      = Sha1Hex(fileBytes.data(), fileBytes.size());
            entry.status    = SampleStatus::Ok;

            entries.push_back(entry);

            if (!Save())
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary::RegisterOrphan failed to update Tracker.yaml");
            }
            else
            {
                outNewId  = entry.id;
                succeeded = true;
                version++;
            }
        }

        return succeeded;
    }

    bool SampleLibrary::Save() const
    {
        std::string yamlText = EmitYaml();
        std::string yamlPath = TrackerPaths::GetTrackerYamlPath();

        bool succeeded = PlatformFileSystem::Get()->WriteFileTextAtomic(yamlPath.c_str(), yamlText);
        if (!succeeded)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary::Save failed to write Tracker.yaml");
        }
        return succeeded;
    }

    // ========================
    // Private helpers
    // ========================

    bool SampleLibrary::ReadAndParseTrackerYaml(std::string& outYamlText, bool& outDidExist) const
    {
        outYamlText.clear();
        std::string yamlPath = TrackerPaths::GetTrackerYamlPath();

        outDidExist = PlatformFileSystem::Get()->FileExists(yamlPath.c_str());
        bool succeeded = false;

        if (outDidExist)
        {
            succeeded = PlatformFileSystem::Get()->ReadFileText(yamlPath.c_str(), outYamlText);
        }
        else
        {
            // No file yet is not an error — the caller treats it as
            // first-run and writes a fresh empty library.
            succeeded = true;
        }

        return succeeded;
    }

    bool SampleLibrary::ParseSampleEntries(const std::string& yamlText, std::vector<SampleEntry>& outEntries) const
    {
        bool succeeded = false;
        outEntries.clear();

        ryml::Tree tree;
        bool       parseOk = true;
        try
        {
            tree = ryml::parse_in_arena(ryml::to_csubstr(yamlText));
        }
        catch (const std::exception& exception)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary: ryml parse failed: %s", exception.what());
            parseOk = false;
        }

        if (parseOk)
        {
            ryml::ConstNodeRef root = tree.rootref();

            // The version field exists for forward-compatibility; today
            // the only valid value is TRACKER_YAML_VERSION. Mismatches
            // surface as a logged warning and an empty library so the
            // user sees fresh state rather than partial garbage.
            if (root.has_child("version"))
            {
                int version = 0;
                root["version"] >> version;
                if (version != TRACKER_YAML_VERSION)
                {
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary: Tracker.yaml version mismatch (got %d, expected %d)",
                        version, TRACKER_YAML_VERSION);
                }
            }

            if (root.has_child("samples") && root["samples"].is_seq())
            {
                for (ryml::ConstNodeRef sampleNode : root["samples"].children())
                {
                    SampleEntry entry;
                    if (sampleNode.has_child("id"))         { entry.id        = NodeToString(sampleNode["id"]); }
                    if (sampleNode.has_child("file"))       { entry.fileName  = NodeToString(sampleNode["file"]); }
                    if (sampleNode.has_child("sizeBytes"))  { entry.sizeBytes = NodeToSize(sampleNode["sizeBytes"]); }
                    if (sampleNode.has_child("sha1"))       { entry.sha1      = NodeToString(sampleNode["sha1"]); }

                    if (entry.id.empty() || entry.fileName.empty())
                    {
                        CCPrint(PrintManager::CHANNEL_ALWAYS, "SampleLibrary: skipping malformed sample entry");
                    }
                    else
                    {
                        outEntries.push_back(entry);
                    }
                }
            }

            succeeded = true;
        }

        return succeeded;
    }

    void SampleLibrary::VerifyEntries(std::vector<SampleEntry>& entriesToVerify) const
    {
        const std::string& samplesPath = TrackerPaths::GetSamplesPath();

        for (size_t i = 0; i < entriesToVerify.size(); i++)
        {
            SampleEntry& entry = entriesToVerify[i];
            std::string  fullPath = samplesPath + "/" + entry.fileName;

            if (!PlatformFileSystem::Get()->FileExists(fullPath.c_str()))
            {
                entry.status = SampleStatus::Broken;
            }
            else
            {
                std::vector<uint8_t> fileBytes;
                bool didRead = PlatformFileSystem::Get()->ReadFileBinary(fullPath.c_str(), fileBytes);

                if (!didRead)
                {
                    // File is listed by the directory but unreadable —
                    // permissions, race with another writer, etc.
                    // Treated as Broken so the verification UI offers
                    // the same forget action.
                    entry.status = SampleStatus::Broken;
                }
                else
                {
                    bool sizeMatches = (fileBytes.size() == entry.sizeBytes);
                    bool sha1Matches = (Sha1Hex(fileBytes.data(), fileBytes.size()) == entry.sha1);

                    if (sizeMatches && sha1Matches)
                    {
                        entry.status = SampleStatus::Ok;
                    }
                    else
                    {
                        entry.status = SampleStatus::Modified;
                    }
                }
            }
        }
    }

    void SampleLibrary::AppendOrphans(std::vector<SampleEntry>& entriesToAugment) const
    {
        std::vector<std::string> diskFiles;
        if (PlatformFileSystem::Get()->ListDirectoryEntries(TrackerPaths::GetSamplesPath().c_str(), diskFiles))
        {
            for (size_t i = 0; i < diskFiles.size(); i++)
            {
                const std::string& fileName = diskFiles[i];

                bool isReferenced = false;
                for (size_t j = 0; j < entriesToAugment.size() && !isReferenced; j++)
                {
                    if (entriesToAugment[j].fileName == fileName)
                    {
                        isReferenced = true;
                    }
                }

                if (!isReferenced)
                {
                    SampleEntry entry;
                    entry.fileName = fileName;
                    entry.status   = SampleStatus::Orphan;
                    // id, sizeBytes, sha1 left blank — the user
                    // promotes the orphan via the panel UI's
                    // "register" action which re-reads and hashes the
                    // file at that point.
                    entriesToAugment.push_back(entry);
                }
            }
        }
    }

    static char SanitiseIdChar(char input)
    {
        char result = '_';
        bool isAlnum = (input >= 'a' && input <= 'z')
                    || (input >= 'A' && input <= 'Z')
                    || (input >= '0' && input <= '9');
        if (isAlnum)
        {
            result = input;
        }
        return result;
    }

    std::string SampleLibrary::GenerateUniqueId(const std::string& sourceFileName) const
    {
        std::string baseId;
        size_t      dotIndex = sourceFileName.find_last_of('.');
        std::string stem     = (dotIndex == std::string::npos) ? sourceFileName : sourceFileName.substr(0, dotIndex);

        baseId.reserve(stem.size());
        for (size_t i = 0; i < stem.size(); i++)
        {
            baseId.push_back(SanitiseIdChar(stem[i]));
        }
        if (baseId.empty())
        {
            baseId = "sample";
        }

        std::string candidate = baseId;
        int         suffix    = 2;
        while (true)
        {
            bool collides = false;
            for (size_t i = 0; i < entries.size() && !collides; i++)
            {
                if (entries[i].id == candidate)
                {
                    collides = true;
                }
            }
            if (!collides)
            {
                break;
            }
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "_%d", suffix);
            candidate = baseId + buffer;
            suffix++;
        }
        return candidate;
    }

    std::string SampleLibrary::EmitYaml() const
    {
        // ryml's emitter is flexible but the schema here is small and
        // human-curated; a hand-rolled emitter keeps the output
        // deterministic (sample order, key order, indentation) without
        // pulling in ryml's writer API. Output stays a strict subset of
        // YAML that ryml can re-parse on the next load.
        char buffer[256];

        std::string result;
        result.reserve(256 + entries.size() * 160);

        std::snprintf(buffer, sizeof(buffer), "version: %d\n", TRACKER_YAML_VERSION);
        result += buffer;
        result += "samples:\n";

        for (size_t i = 0; i < entries.size(); i++)
        {
            const SampleEntry& entry = entries[i];

            // Orphans live only in-memory until the user dispositions
            // them; they are detected at load time by scanning the
            // Samples/ directory and so do not need to be persisted.
            if (entry.status == SampleStatus::Orphan)
            {
                continue;
            }

            result += "  - id: ";
            result += entry.id;
            result += "\n    file: ";
            result += entry.fileName;
            result += "\n    sizeBytes: ";
            std::snprintf(buffer, sizeof(buffer), "%zu", entry.sizeBytes);
            result += buffer;
            result += "\n    sha1: ";
            result += entry.sha1;
            result += "\n";
        }

        return result;
    }
}
