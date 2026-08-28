#ifndef CCFILE_H
#define CCFILE_H

#include <string>

namespace CCFile
{
    std::string GetDirectoryFromPath(const std::string& filePath);
    std::string GetFilenameNoExtension(const std::string& filePath);
    std::string GetExtension(const std::string& filePath);
}

#endif // CCFILE_H
