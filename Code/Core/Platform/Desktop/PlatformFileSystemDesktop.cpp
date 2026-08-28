#include "PlatformFileSystemDesktop.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <system_error>

namespace CC
{
    PlatformFileSystemDesktop::PlatformFileSystemDesktop()
    {
    }

    PlatformFileSystemDesktop::~PlatformFileSystemDesktop()
    {
    }

    bool PlatformFileSystemDesktop::ReadFileBinary(const char* path, std::vector<uint8_t>& outBuffer) const
    {
        bool succeeded = false;
        outBuffer.clear();

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file.is_open())
        {
            std::streamsize size = file.tellg();
            if (size >= 0)
            {
                file.seekg(0, std::ios::beg);
                outBuffer.resize(static_cast<size_t>(size));
                if (size == 0 || file.read(reinterpret_cast<char*>(outBuffer.data()), size))
                {
                    succeeded = true;
                }
            }
            file.close();
        }

        if (!succeeded)
        {
            outBuffer.clear();
        }
        return succeeded;
    }

    bool PlatformFileSystemDesktop::ReadFileText(const char* path, std::string& outText) const
    {
        bool succeeded = false;
        outText.clear();

        std::ifstream file(path);
        if (file.is_open())
        {
            std::stringstream stream;
            stream << file.rdbuf();
            outText = stream.str();
            succeeded = true;
            file.close();
        }

        return succeeded;
    }

    bool PlatformFileSystemDesktop::WriteFileText(const char* path, const std::string& contents) const
    {
        bool succeeded = false;

        std::ofstream file(path);
        if (file.is_open())
        {
            file << contents;
            succeeded = file.good();
            file.close();
        }

        return succeeded;
    }

    bool PlatformFileSystemDesktop::CopyFile(const char* sourcePath, const char* destinationPath) const
    {
        bool succeeded = false;

        if (sourcePath != nullptr && destinationPath != nullptr)
        {
            std::error_code error;
            std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::none, error);
            if (!error)
            {
                succeeded = true;
            }
        }

        return succeeded;
    }

    bool PlatformFileSystemDesktop::ListDirectoryEntries(const char* directoryPath, std::vector<std::string>& outFileNames) const
    {
        bool succeeded = false;
        outFileNames.clear();

        if (directoryPath != nullptr)
        {
            std::error_code error;
            std::filesystem::directory_iterator iterator(directoryPath, error);
            if (!error)
            {
                std::filesystem::directory_iterator end;
                while (iterator != end)
                {
                    if (iterator->is_regular_file())
                    {
                        outFileNames.push_back(iterator->path().filename().string());
                    }
                    iterator.increment(error);
                    if (error)
                    {
                        break;
                    }
                }
                succeeded = !error;
            }
        }

        if (!succeeded)
        {
            outFileNames.clear();
        }
        return succeeded;
    }

    bool PlatformFileSystemDesktop::WriteFileTextAtomic(const char* path, const std::string& contents) const
    {
        bool succeeded = false;

        if (path != nullptr)
        {
            std::string tempPath = std::string(path) + ".tmp";

            if (WriteFileText(tempPath.c_str(), contents))
            {
                // std::filesystem::rename atomically replaces the
                // destination when source and destination share a
                // filesystem (which they do, since tempPath is a sibling
                // of path).
                std::error_code error;
                std::filesystem::rename(tempPath, path, error);
                if (!error)
                {
                    succeeded = true;
                }
                else
                {
                    std::remove(tempPath.c_str());
                }
            }
        }

        return succeeded;
    }

    bool PlatformFileSystemDesktop::FileExists(const char* path) const
    {
        struct stat info;
        return stat(path, &info) == 0;
    }

    time_t PlatformFileSystemDesktop::GetLastWriteTime(const char* path) const
    {
        time_t result = 0;
        struct stat info;
        if (stat(path, &info) == 0)
        {
            result = info.st_mtime;
        }
        return result;
    }
}
