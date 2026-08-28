#include "ShaderDefinition.h"
#include "CCAssert.h"
#include "PrintManager.h"

namespace CC
{
    ShaderDefinition::ShaderDefinition(const char* shaderPathRoot, const char* shaderFileName)
        : isCompiled(false)
    {
        shaderFilePath = std::string(shaderPathRoot) + std::string(shaderFileName);
    };
            
    ShaderDefinition::~ShaderDefinition() {};
}