#include "CCFile.h"

namespace CCFile
{
    std::string GetDirectoryFromPath(const std::string& filePath)
    {
        size_t lastSlash = filePath.find_last_of("\\/");
        if (lastSlash != std::string::npos)
        {
            return filePath.substr(0, lastSlash + 1);
        }
        return "";
    }

    std::string GetFilenameNoExtension(const std::string& filePath)
    {
        std::string baseName = filePath;
        size_t lastSlash = baseName.find_last_of("\\/");
        if (lastSlash != std::string::npos)
        {
            baseName = baseName.substr(lastSlash + 1);
        }
        size_t dotPos = baseName.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            baseName = baseName.substr(0, dotPos);
        }
        return baseName;
    }

    std::string GetExtension(const std::string& filePath)
    {
        size_t dotPos = filePath.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            return filePath.substr(dotPos);
        }
        return "";
    }
}
