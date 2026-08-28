#ifndef PLATFORMFILESYSTEM_H
#define PLATFORMFILESYSTEM_H

#include <string>
#include <vector>
#include <cstdint>
#include <time.h>

namespace CC
{
    // ========================
    // PlatformFileSystem
    // ========================
    //
    // Abstract base for file I/O across platforms. Concrete backends
    // (PlatformFileSystemDesktop for Windows/Linux/macOS, future
    // PlatformFileSystemAndroid for AAssetManager-backed reads) inherit
    // and override the virtual methods. Singleton: created once during
    // CoreMain::Init, retrieved via Get().

    class PlatformFileSystem
    {
    public:
        virtual ~PlatformFileSystem();

        static PlatformFileSystem* Get();

        virtual bool ReadFileBinary(const char* path, std::vector<uint8_t>& outBuffer) const = 0;
        virtual bool ReadFileText(const char* path, std::string& outText) const = 0;
        virtual bool WriteFileText(const char* path, const std::string& contents) const = 0;

        // Writes contents to a sibling temp file, then renames it over
        // the destination in a single filesystem operation. Either
        // readers see the previous file or the new one, never an
        // intermediate half-written state — important for files the
        // app reads back at startup (Tracker.yaml, .cctrack project
        // files) where corruption from a crash mid-save would cost
        // user data.
        virtual bool WriteFileTextAtomic(const char* path, const std::string& contents) const = 0;

        // Byte-for-byte copy from sourcePath to destinationPath. The
        // intent of preserving file format (e.g. WAV/OGG sample files
        // retain their compressed bytes at rest) is what makes this
        // its own primitive rather than a Read+Write round-trip.
        // Returns false if the destination already exists.
        virtual bool CopyFile(const char* sourcePath, const char* destinationPath) const = 0;

        // Lists the file basenames (no path component) directly inside
        // directoryPath. Sub-directories are not recursed into and not
        // returned. Out-vector is populated whether the call succeeds
        // or fails; failures clear it.
        virtual bool ListDirectoryEntries(const char* directoryPath, std::vector<std::string>& outFileNames) const = 0;

        virtual bool FileExists(const char* path) const = 0;
        virtual time_t GetLastWriteTime(const char* path) const = 0;

    protected:
        PlatformFileSystem();

    private:
        static PlatformFileSystem* instance;
    };
}

#endif // PLATFORMFILESYSTEM_H
