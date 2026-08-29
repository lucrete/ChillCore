#include "ShaderParamLayout.h"

#include <sstream>

#include "PrintManager.h"

namespace CC
{
    static const char* MATERIAL_PARAMS_DECLARATION = "uniform MaterialParams";
    static const int   STD140_BLOCK_ALIGNMENT_BYTES = 16;

    void ShaderParamLayout::Clear()
    {
        for (int i = 0; i < MAX_PARAMS; i++)
        {
            params[i] = ShaderParam();
        }
        paramCount     = 0;
        blockSizeBytes = 0;
    }

    void ShaderParamLayout::ParseFromFragmentSource(const std::string& fragmentSource)
    {
        Clear();

        std::string blockBody;
        if (ExtractBlockBody(fragmentSource, blockBody))
        {
            std::string declarations = StripLineComments(blockBody);
            bool isLayoutValid = true;

            size_t statementStart = 0;
            while (statementStart < declarations.length() && isLayoutValid)
            {
                size_t statementEnd = declarations.find(';', statementStart);
                if (statementEnd == std::string::npos)
                {
                    statementStart = declarations.length();
                }
                else
                {
                    std::string statement = declarations.substr(statementStart, statementEnd - statementStart);
                    std::istringstream statementStream(statement);

                    std::string typeName;
                    std::string paramName;
                    std::string trailingToken;
                    statementStream >> typeName >> paramName >> trailingToken;

                    if (typeName.empty() || paramName.empty())
                    {
                        // Blank line or trailing whitespace between members.
                    }
                    else if (!trailingToken.empty())
                    {
                        CCPrint(PrintManager::CHANNEL_WARN,
                                "MaterialParams member is not a simple 'type name;' declaration: %s",
                                statement.c_str());
                        isLayoutValid = false;
                    }
                    else
                    {
                        isLayoutValid = AddParam(typeName, paramName);
                    }

                    statementStart = statementEnd + 1;
                }
            }

            blockSizeBytes = (blockSizeBytes + STD140_BLOCK_ALIGNMENT_BYTES - 1)
                           / STD140_BLOCK_ALIGNMENT_BYTES * STD140_BLOCK_ALIGNMENT_BYTES;

            if (blockSizeBytes > MAX_BLOCK_SIZE_BYTES)
            {
                CCPrint(PrintManager::CHANNEL_WARN,
                        "MaterialParams block is %d bytes, over the %d byte limit. Block ignored.",
                        blockSizeBytes, MAX_BLOCK_SIZE_BYTES);
                isLayoutValid = false;
            }

            // A member this parser cannot place leaves every later offset
            // disagreeing with the driver's own std140 layout, so a partial
            // parse is worse than none: drop the block and let the shader
            // read its default-initialised values.
            if (!isLayoutValid)
            {
                Clear();
            }
        }
    }

    const ShaderParam* ShaderParamLayout::FindParam(const std::string& name) const
    {
        const ShaderParam* result = nullptr;
        for (int i = 0; i < paramCount && result == nullptr; i++)
        {
            if (params[i].name == name)
            {
                result = &params[i];
            }
        }
        return result;
    }

    bool ShaderParamLayout::ExtractBlockBody(const std::string& source, std::string& outBlockBody) const
    {
        bool result = false;

        size_t declarationPos = source.find(MATERIAL_PARAMS_DECLARATION);
        if (declarationPos != std::string::npos)
        {
            size_t openBrace = source.find('{', declarationPos);
            if (openBrace != std::string::npos)
            {
                size_t closeBrace = source.find('}', openBrace);
                if (closeBrace != std::string::npos)
                {
                    outBlockBody = source.substr(openBrace + 1, closeBrace - openBrace - 1);
                    result = true;
                }
                else
                {
                    CCPrint(PrintManager::CHANNEL_WARN, "MaterialParams block has no closing brace.");
                }
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_WARN, "MaterialParams block has no opening brace.");
            }
        }

        return result;
    }

    bool ShaderParamLayout::AddParam(const std::string& typeName, const std::string& paramName)
    {
        bool            result    = true;
        ShaderParamType type      = ShaderParamType::Float;
        int             alignment = 0;
        int             size      = 0;

        if (paramCount >= MAX_PARAMS)
        {
            CCPrint(PrintManager::CHANNEL_WARN,
                    "MaterialParams holds at most %d members. Dropped: %s",
                    MAX_PARAMS, paramName.c_str());
            result = false;
        }
        else if (!GetTypeInfo(typeName, type, alignment, size))
        {
            CCPrint(PrintManager::CHANNEL_WARN,
                    "MaterialParams member type is not supported: %s %s",
                    typeName.c_str(), paramName.c_str());
            result = false;
        }
        else
        {
            int offset = (blockSizeBytes + alignment - 1) / alignment * alignment;

            params[paramCount].name        = paramName;
            params[paramCount].type        = type;
            params[paramCount].offsetBytes = offset;
            params[paramCount].sizeBytes   = size;
            paramCount++;

            blockSizeBytes = offset + size;
        }

        return result;
    }

    bool ShaderParamLayout::GetTypeInfo(const std::string& typeName, ShaderParamType& outType,
                                        int& outAlignmentBytes, int& outSizeBytes)
    {
        bool result = true;

        if (typeName == "float")
        {
            outType = ShaderParamType::Float;
            outAlignmentBytes = 4;
            outSizeBytes      = 4;
        }
        else if (typeName == "int")
        {
            outType = ShaderParamType::Int;
            outAlignmentBytes = 4;
            outSizeBytes      = 4;
        }
        else if (typeName == "vec2")
        {
            outType = ShaderParamType::Vector2;
            outAlignmentBytes = 8;
            outSizeBytes      = 8;
        }
        else if (typeName == "vec3")
        {
            outType = ShaderParamType::Vector3;
            outAlignmentBytes = 16;
            outSizeBytes      = 12;
        }
        else if (typeName == "vec4")
        {
            outType = ShaderParamType::Vector4;
            outAlignmentBytes = 16;
            outSizeBytes      = 16;
        }
        else if (typeName == "mat4")
        {
            outType = ShaderParamType::Mat4;
            outAlignmentBytes = 16;
            outSizeBytes      = 64;
        }
        else
        {
            result = false;
        }

        return result;
    }

    std::string ShaderParamLayout::StripLineComments(const std::string& text)
    {
        std::stringstream result;
        std::stringstream lines(text);
        std::string line;

        while (std::getline(lines, line))
        {
            size_t commentStart = line.find("//");
            if (commentStart != std::string::npos)
            {
                line.erase(commentStart);
            }
            result << line << '\n';
        }

        return result.str();
    }
}
