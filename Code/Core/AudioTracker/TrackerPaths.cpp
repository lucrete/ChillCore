#include "TrackerPaths.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

#include "PrintManager.h"

namespace CC
{
    namespace TrackerPaths
    {
        // Cached on first resolution. Computing the root involves an
        // environment lookup and a directory create; a single
        // application session always resolves to the same path.
        static std::string cachedRootPath;
        static std::string cachedSamplesPath;
        static std::string cachedProjectsPath;

        // MSVC marks std::getenv as deprecated under its secure-CRT
        // scheme and recommends _dupenv_s. Wrap the platform difference
        // in one place so the call sites stay clean.
        static std::string GetEnvironmentVariable(const char* name)
        {
            std::string result;
#ifdef _WIN32
            char*  value     = nullptr;
            size_t valueSize = 0;
            if (_dupenv_s(&value, &valueSize, name) == 0 && value != nullptr)
            {
                result = value;
                std::free(value);
            }
#else
            const char* value = std::getenv(name);
            if (value != nullptr)
            {
                result = value;
            }
#endif
            return result;
        }

        static std::string ResolveRootPath()
        {
            std::string result;

#ifdef _WIN32
            std::string programData = GetEnvironmentVariable("PROGRAMDATA");
            if (!programData.empty())
            {
                result = programData + "/ChillCore/Tracker";
            }
            else
            {
                result = "C:/ProgramData/ChillCore/Tracker";
            }
#else
            std::string home = GetEnvironmentVariable("HOME");
            if (!home.empty())
            {
                result = home + "/.local/share/ChillCore/Tracker";
            }
            else
            {
                result = "/tmp/ChillCore/Tracker";
            }
#endif

            return result;
        }

        static void EnsureDirectoryExists(const std::string& path)
        {
            std::error_code error;
            std::filesystem::create_directories(path, error);
            if (error)
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerPaths: failed to create directory: %s", path.c_str());
            }
        }

        const std::string& GetRootPath()
        {
            if (cachedRootPath.empty())
            {
                cachedRootPath = ResolveRootPath();
                EnsureDirectoryExists(cachedRootPath);
            }
            return cachedRootPath;
        }

        const std::string& GetSamplesPath()
        {
            if (cachedSamplesPath.empty())
            {
                cachedSamplesPath = GetRootPath() + "/Samples";
                EnsureDirectoryExists(cachedSamplesPath);
            }
            return cachedSamplesPath;
        }

        const std::string& GetProjectsPath()
        {
            if (cachedProjectsPath.empty())
            {
                cachedProjectsPath = GetRootPath() + "/Projects";
                EnsureDirectoryExists(cachedProjectsPath);
            }
            return cachedProjectsPath;
        }

        std::string GetTrackerYamlPath()
        {
            return GetRootPath() + "/Tracker.yaml";
        }
    }
}
