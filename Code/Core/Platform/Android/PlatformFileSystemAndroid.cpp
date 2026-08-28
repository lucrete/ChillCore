#include "PlatformFileSystemAndroid.h"
#include "AndroidAppContext.h"
#include "CCAssert.h"

#include <android/asset_manager.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <fstream>
#include <sstream>

namespace CC
{
    PlatformFileSystemAndroid::PlatformFileSystemAndroid()
    {
        android_app* pApp = GetAndroidApp();
        CC_ASSERT(pApp != nullptr, "AndroidAppContext not initialised — call SetAndroidApp before PlatformFileSystem creation");

        assetManager = pApp->activity->assetManager;
        if (pApp->activity->internalDataPath != nullptr)
        {
            internalDataPath = pApp->activity->internalDataPath;
        }
    }

    PlatformFileSystemAndroid::~PlatformFileSystemAndroid()
    {
    }

    // Asset paths in the engine use Windows-style backslashes (e.g.
    // "Data\\Shaders\\defaultBasic.glsl"). AAssetManager wants
    // forward-slash paths relative to the APK's assets/ root. The
    // engine's Gradle config maps Code/App/Data -> assets/, so a path
    // like "Data\\Shaders\\foo.glsl" resolves to the asset path
    // "Shaders/foo.glsl" once the leading "Data\" is stripped and
    // separators are normalised.
    static std::string NormaliseAssetPath(const char* path)
    {
        std::string result;
        if (path != nullptr)
        {
            result = path;
            for (size_t i = 0; i < result.length(); i++)
            {
                if (result[i] == '\\')
                {
                    result[i] = '/';
                }
            }

            // Strip leading "Data/" since assets ARE the contents of Data/.
            const char* prefix = "Data/";
            const size_t prefixLen = 5;
            if (result.length() >= prefixLen && result.compare(0, prefixLen, prefix) == 0)
            {
                result.erase(0, prefixLen);
            }
        }
        return result;
    }

    bool PlatformFileSystemAndroid::ReadFileBinary(const char* path, std::vector<uint8_t>& outBuffer) const
    {
        bool succeeded = false;
        outBuffer.clear();

        if (assetManager != nullptr)
        {
            std::string assetPath = NormaliseAssetPath(path);
            AAsset* asset = AAssetManager_open(assetManager, assetPath.c_str(), AASSET_MODE_BUFFER);
            if (asset != nullptr)
            {
                off_t size = AAsset_getLength(asset);
                if (size > 0)
                {
                    outBuffer.resize(static_cast<size_t>(size));
                    int readBytes = AAsset_read(asset, outBuffer.data(), static_cast<size_t>(size));
                    succeeded = (readBytes == size);
                }
                else if (size == 0)
                {
                    succeeded = true;
                }
                AAsset_close(asset);
            }
        }

        if (!succeeded)
        {
            outBuffer.clear();
        }
        return succeeded;
    }

    bool PlatformFileSystemAndroid::ReadFileText(const char* path, std::string& outText) const
    {
        std::vector<uint8_t> bytes;
        bool succeeded = ReadFileBinary(path, bytes);
        if (succeeded)
        {
            // Match Desktop's text-mode behaviour: collapse CRLF to LF.
            // AAssetManager returns raw bytes, so files authored on
            // Windows leave a trailing '\r' on every line, which breaks
            // parsers that read the rest of a line as a value (e.g.
            // OBJ/MTL map_Kd -> "gumpwave.png\r").
            outText.clear();
            outText.reserve(bytes.size());
            for (size_t readIndex = 0; readIndex < bytes.size(); readIndex++)
            {
                char ch = static_cast<char>(bytes[readIndex]);
                bool isCrBeforeLf = (ch == '\r' && readIndex + 1 < bytes.size() && bytes[readIndex + 1] == '\n');
                if (!isCrBeforeLf)
                {
                    outText.push_back(ch);
                }
            }
        }
        else
        {
            outText.clear();
        }
        return succeeded;
    }

    bool PlatformFileSystemAndroid::WriteFileText(const char* path, const std::string& contents) const
    {
        bool succeeded = false;
        if (!internalDataPath.empty() && path != nullptr)
        {
            std::string fullPath = internalDataPath + "/" + path;
            std::ofstream file(fullPath);
            if (file.is_open())
            {
                file << contents;
                succeeded = file.good();
                file.close();
            }
        }
        return succeeded;
    }

    // AudioTracker (the only consumer of atomic writes today) is
    // desktop-only, so this Android override exists only to satisfy the
    // virtual interface and is not expected to be called. It falls back
    // to the plain non-atomic write so that any future Android consumer
    // gets correct content semantics; the temp+rename atomicity story
    // would need separate Android-specific work (scoped storage, etc.)
    // before relying on it.
    bool PlatformFileSystemAndroid::WriteFileTextAtomic(const char* path, const std::string& contents) const
    {
        return WriteFileText(path, contents);
    }

    // CopyFile and ListDirectoryEntries are stubs because their only
    // consumer today (AudioTracker's sample import + library
    // verification) is desktop-only. Real Android implementations
    // would need to translate between AAssetManager paths and
    // internalDataPath; that work waits for a real Android consumer.
    bool PlatformFileSystemAndroid::CopyFile(const char* sourcePath, const char* destinationPath) const
    {
        (void)sourcePath;
        (void)destinationPath;
        return false;
    }

    bool PlatformFileSystemAndroid::ListDirectoryEntries(const char* directoryPath, std::vector<std::string>& outFileNames) const
    {
        (void)directoryPath;
        outFileNames.clear();
        return false;
    }

    bool PlatformFileSystemAndroid::FileExists(const char* path) const
    {
        bool exists = false;
        if (assetManager != nullptr)
        {
            std::string assetPath = NormaliseAssetPath(path);
            AAsset* asset = AAssetManager_open(assetManager, assetPath.c_str(), AASSET_MODE_UNKNOWN);
            if (asset != nullptr)
            {
                exists = true;
                AAsset_close(asset);
            }
        }
        return exists;
    }

    time_t PlatformFileSystemAndroid::GetLastWriteTime(const char* path) const
    {
        (void)path;
        // APK assets are immutable post-build; a fixed sentinel value is
        // returned so the engine's hot-reload check (compares this to a
        // stored last-write-time) never triggers a recompile on Android.
        return 1;
    }
}
