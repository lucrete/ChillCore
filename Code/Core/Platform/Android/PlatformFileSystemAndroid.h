#ifndef PLATFORMFILESYSTEMANDROID_H
#define PLATFORMFILESYSTEMANDROID_H

#include "PlatformFileSystem.h"

struct AAssetManager;

namespace CC
{
    // ========================
    // PlatformFileSystemAndroid
    // ========================
    //
    // Reads come from the APK's asset stream via AAssetManager (the
    // Gradle build packages Code/App/Data into the APK's assets/ dir).
    // Writes go to the activity's internalDataPath (private app
    // storage). The android_app context, including the asset manager
    // and internal-data path, is retrieved via AndroidAppContext.

    class PlatformFileSystemAndroid : public PlatformFileSystem
    {
    public:
        PlatformFileSystemAndroid();
        virtual ~PlatformFileSystemAndroid();

        bool   ReadFileBinary(const char* path, std::vector<uint8_t>& outBuffer) const override;
        bool   ReadFileText(const char* path, std::string& outText) const override;
        bool   WriteFileText(const char* path, const std::string& contents) const override;
        bool   WriteFileTextAtomic(const char* path, const std::string& contents) const override;
        bool   CopyFile(const char* sourcePath, const char* destinationPath) const override;
        bool   ListDirectoryEntries(const char* directoryPath, std::vector<std::string>& outFileNames) const override;
        bool   FileExists(const char* path) const override;
        time_t GetLastWriteTime(const char* path) const override;

    private:
        AAssetManager* assetManager       = nullptr;
        std::string    internalDataPath;
    };
}

#endif // PLATFORMFILESYSTEMANDROID_H
