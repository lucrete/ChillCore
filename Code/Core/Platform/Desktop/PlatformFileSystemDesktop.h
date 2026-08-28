#ifndef PLATFORMFILESYSTEMDESKTOP_H
#define PLATFORMFILESYSTEMDESKTOP_H

#include "PlatformFileSystem.h"

namespace CC
{
    // ========================
    // PlatformFileSystemDesktop
    // ========================
    //
    // Desktop (Windows / Linux / macOS) PlatformFileSystem backend.
    // Reads via std::ifstream, writes via std::ofstream, queries via
    // POSIX stat. Used for any platform whose working directory is a
    // real on-disk filesystem.

    class PlatformFileSystemDesktop : public PlatformFileSystem
    {
    public:
        PlatformFileSystemDesktop();
        ~PlatformFileSystemDesktop() override;

        bool ReadFileBinary(const char* path, std::vector<uint8_t>& outBuffer) const override;
        bool ReadFileText(const char* path, std::string& outText) const override;
        bool WriteFileText(const char* path, const std::string& contents) const override;
        bool WriteFileTextAtomic(const char* path, const std::string& contents) const override;
        bool CopyFile(const char* sourcePath, const char* destinationPath) const override;
        bool ListDirectoryEntries(const char* directoryPath, std::vector<std::string>& outFileNames) const override;
        bool FileExists(const char* path) const override;
        time_t GetLastWriteTime(const char* path) const override;
    };
}

#endif // PLATFORMFILESYSTEMDESKTOP_H
