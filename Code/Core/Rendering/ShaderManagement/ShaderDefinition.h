#ifndef SHADERDEFINITION_H
#define SHADERDEFINITION_H

#include <string>
#include <ctime>
#include "GfxHandles.h"
#include "CustomParamLayout.h"

namespace CC
{
    class ShaderDefinition
    {
    public:
        ShaderDefinition() {};
        ShaderDefinition(const char* shaderPathRoot, const char* shaderFileName);
        virtual ~ShaderDefinition();

        std::string shaderFilePath;
        std::string vertexShaderText;
        std::string fragmentShaderText;
        time_t lastWriteTime = 0;
        Gfx::ShaderHandle shaderHandle;
        CustomParamLayout customParamLayout;
        bool isCompiled = false;
    };
}

#endif // SHADERDEFINITION_H
