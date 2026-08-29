#ifndef SHADERMANAGER_H
#define SHADERMANAGER_H

#include "ShaderDefinition.h"
#include <map>
#include <string>

namespace CC
{
    class ShaderManager
    {
    public:
        ShaderManager();
        virtual ~ShaderManager();
        static ShaderManager* Get();
        void Update();

        Gfx::ShaderHandle GetShaderHandle(const std::string& shaderName);

        // Layout of the shader-declared CustomParams block. Null when the
        // shader is unknown; empty when it declares no block.
        const CustomParamLayout* GetCustomParamLayout(const std::string& shaderName) const;

        void CompileShader(const std::string& shaderName);
        bool IsShaderCompiled(const std::string& shaderName);

    private:
        void CompileShaders();
        void ParseShader(ShaderDefinition* shaderDef);
        std::string ProcessInclude(const std::string& includePath);
        bool ExtractIncludePath(const std::string& line, std::string& outIncludePath);
        void CompileShader(ShaderDefinition* shaderDef);
        bool HasFileChanged(const std::string& shaderName);

        static ShaderManager* instance;
        static const char* shaderBasePath;
        std::map<std::string, ShaderDefinition*> shaderMap;
        std::string hotloadShader;
    };
}

#endif // SHADERMANAGER_H
