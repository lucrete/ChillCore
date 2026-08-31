#include "ShaderManager.h"
#include "CCAssert.h"
#include "CCFile.h"
#include "PrintManager.h"
#include "InputManager.h"
#include "MaterialManager.h"
#include "GfxRenderApi.h"
#include "GfxDescriptions.h"
#include "PlatformFileSystem.h"

#include <string>
#include <sstream>
#include <iostream>

namespace
{
    // ========================
    // InjectShaderPreamble
    // ========================
    //
    // Re-targets shader source to the active backend's GLSL dialect.
    // - Desktop GL (default, no CC_GFX_BACKEND_GLES define): no-op.
    //   Source already conforms to GLSL 330 core / 430 core.
    // - GLES (CC_GFX_BACKEND_GLES defined): strips the existing
    //   #version line and prepends `#version 310 es` plus default
    //   precision qualifiers. Multiple #version directives are illegal
    //   in GLSL so the original must be removed, not just shadowed.
    //
    // Called from CompileShader after #include resolution, so the
    // injected preamble lands at the very top of the source string
    // submitted to RenderApi::CreateShader.
    //
    // The plumbing is permanent in this translation unit; the GLES
    // branch lights up only when CC_GFX_BACKEND_GLES is defined by the
    // Android build (Phase B onward).
    void InjectShaderPreamble(std::string& source, bool isFragmentStage)
    {
#ifdef CC_GFX_BACKEND_GLES
        size_t versionPos = source.find("#version");
        if (versionPos != std::string::npos)
        {
            size_t lineEnd = source.find('\n', versionPos);
            if (lineEnd != std::string::npos)
            {
                source.erase(versionPos, lineEnd - versionPos + 1);
            }
            else
            {
                source.erase(versionPos);
            }
        }

        std::string preamble = "#version 310 es\n";
        preamble += "precision highp float;\n";
        preamble += "precision highp int;\n";
        if (isFragmentStage)
        {
            preamble += "precision highp sampler2D;\n";
        }
        source.insert(0, preamble);
#else
        (void)source;
        (void)isFragmentStage;
#endif
    }
}

namespace CC
{
    ShaderManager* ShaderManager::instance = NULL;
    const char* ShaderManager::shaderBasePath = "Data\\Shaders\\";

    ShaderManager::ShaderManager()
    {
        CC_ASSERT(instance == NULL, "ShaderManager already created");
        instance = this;

        shaderMap["DefaultBasic"] = new ShaderDefinition(shaderBasePath, "defaultBasic.glsl");
        shaderMap["DefaultError"] = new ShaderDefinition(shaderBasePath, "defaultError.glsl");
        shaderMap["FullScreenBlit"] = new ShaderDefinition(shaderBasePath, "Rendering\\fullScreenBlit.glsl");
        shaderMap["FullScreenFade"] = new ShaderDefinition(shaderBasePath, "Rendering\\fullScreenFade.glsl");
        shaderMap["FullScreenQuad"] = new ShaderDefinition(shaderBasePath, "Rendering\\fullScreenQuad.glsl");
        shaderMap["PostProcessUber"] = new ShaderDefinition(shaderBasePath, "Rendering\\postProcessUber.glsl");
        shaderMap["PostProcessBloomPrefilter"] = new ShaderDefinition(shaderBasePath, "Rendering\\postProcessBloomPrefilter.glsl");
        shaderMap["PostProcessBloomBlur"] = new ShaderDefinition(shaderBasePath, "Rendering\\postProcessBloomBlur.glsl");
        shaderMap["GradientViewer"] = new ShaderDefinition(shaderBasePath, "ProceduralArt\\gradientViewer.glsl");
        shaderMap["UnitCircleRipples"] = new ShaderDefinition(shaderBasePath, "ProceduralArt\\unitCircleRipples.glsl");
        shaderMap["RadialWaves"] = new ShaderDefinition(shaderBasePath, "ProceduralArt\\radialWaves.glsl");
        shaderMap["TrigWaves"] = new ShaderDefinition(shaderBasePath, "ProceduralArt\\trigWaves.glsl");
        shaderMap["Fractal2d"] = new ShaderDefinition(shaderBasePath, "ProceduralArt\\mandelbrot2d.glsl");
        shaderMap["TextureShader"] = new ShaderDefinition(shaderBasePath, "Rendering\\texture.glsl");
        shaderMap["LitColour"] = new ShaderDefinition(shaderBasePath, "Rendering\\litColour.glsl");
        shaderMap["Pbr"] = new ShaderDefinition(shaderBasePath, "Rendering\\pbr.glsl");
        shaderMap["MsdfText"] = new ShaderDefinition(shaderBasePath, "Ui\\msdfText.glsl");
        shaderMap["UiQuad"] = new ShaderDefinition(shaderBasePath, "Ui\\uiQuad.glsl");
        CompileShaders();

        hotloadShader = "UnitCircleRipples";
    }

    ShaderManager::~ShaderManager()
    {
        instance = NULL;
    }

    ShaderManager* ShaderManager::Get()
    {
        CC_ASSERT(instance != NULL, "ShaderManager not created yet");
        return instance;
    }

    void ShaderManager::Update()
    {
        if (!hotloadShader.empty() && HasFileChanged(hotloadShader))
        {
            CompileShader(hotloadShader);
            if (shaderMap[hotloadShader]->isCompiled)
            {
                MaterialManager::Get()->ReconnectShader(hotloadShader);
                CCPrint(PrintManager::CHANNEL_SHADER, "Shader compiled: %s", shaderMap[hotloadShader]->shaderFilePath.c_str());
            }
        }
    }

    void ShaderManager::CompileShaders()
    {
        for (auto i = shaderMap.begin(); i != shaderMap.end(); i++)
        {
            CompileShader(i->second);
        }
    }

    Gfx::ShaderHandle ShaderManager::GetShaderHandle(const std::string& shaderName)
    {
        Gfx::ShaderHandle result;
        auto it = shaderMap.find(shaderName);
        CC_ASSERT(it != shaderMap.end(), "Shader not found: " + shaderName);
        if (it != shaderMap.end())
        {
            result = it->second->isCompiled ? it->second->shaderHandle : shaderMap["DefaultError"]->shaderHandle;
        }
        return result;
    }

    const CustomParamLayout* ShaderManager::GetCustomParamLayout(const std::string& shaderName) const
    {
        const CustomParamLayout* result = nullptr;
        auto it = shaderMap.find(shaderName);
        if (it != shaderMap.end())
        {
            result = &it->second->customParamLayout;
        }
        return result;
    }

    bool ShaderManager::ExtractIncludePath(const std::string& line, std::string& outIncludePath)
    {
        const std::string includeKeyword = "#include";
        size_t includePos = line.find(includeKeyword);
        if (includePos == std::string::npos)
            return false;

        // Find the first quote after #include, allowing for any number of spaces
        size_t searchPos = includePos + includeKeyword.length();
        size_t startQuote = std::string::npos;

        // Skip any whitespace
        while (searchPos < line.length()) {
            if (line[searchPos] == ' ' || line[searchPos] == '\t') {
                searchPos++;
            }
            else if (line[searchPos] == '\"') {
                startQuote = searchPos;
                break;
            }
            else {
                // Found non-whitespace, non-quote character after #include
                CCPrint(PrintManager::CHANNEL_WARN, "Invalid include format: No opening quote after #include in: %s", line.c_str());
                return false;
            }
        }

        // If we didn't find a quote, return false
        if (startQuote == std::string::npos) {
            CCPrint(PrintManager::CHANNEL_WARN, "Invalid include format: No opening quote found in: %s", line.c_str());
            return false;
        }

        size_t endQuote = line.find("\"", startQuote + 1);
        if (endQuote == std::string::npos) {
            CCPrint(PrintManager::CHANNEL_WARN, "Invalid include format: No closing quote found in: %s", line.c_str());
            return false;
        }

        outIncludePath = line.substr(startQuote + 1, endQuote - startQuote - 1);
        if (outIncludePath.empty()) {
            CCPrint(PrintManager::CHANNEL_WARN, "Invalid include: Empty path in: %s", line.c_str());
            return false;
        }

        return true;
    }

    std::string ShaderManager::ProcessInclude(const std::string& includePath)
    {
        // All includes are relative to the base shader path
        std::string fullPath = shaderBasePath + includePath;

        // Normalize path separators
        for (size_t i = 0; i < fullPath.length(); i++)
        {
            if (fullPath[i] == '/') fullPath[i] = '\\';
        }

        std::string output;
        std::string fileContents;
        if (PlatformFileSystem::Get()->ReadFileText(fullPath.c_str(), fileContents))
        {
            std::stringstream result;
            std::stringstream includeContent(fileContents);
            std::string line;

            while (std::getline(includeContent, line))
            {
                std::string nestedIncludePath;
                if (ExtractIncludePath(line, nestedIncludePath))
                {
                    result << ProcessInclude(nestedIncludePath);
                }
                else
                {
                    result << line << std::endl;
                }
            }

            output = result.str();
        }
        else
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to open included file: %s", fullPath.c_str());
            output = "// Failed to include file: " + includePath + "\n";
        }

        return output;
    }

    void ShaderManager::ParseShader(ShaderDefinition* shaderDef)
    {
        std::string shaderCode;
        if (PlatformFileSystem::Get()->ReadFileText(shaderDef->shaderFilePath.c_str(), shaderCode))
        {
            std::stringstream shaderCodeStream(shaderCode);

            std::string line;
            std::stringstream vertexStream;
            std::stringstream fragmentStream;
            bool isVertex = false;
            bool isFragment = false;

            while (std::getline(shaderCodeStream, line)) {
                // Check for shader section markers
                if (line.find("#shader vertex") != std::string::npos) {
                    isVertex = true;
                    isFragment = false;
                    continue;
                }
                else if (line.find("#shader fragment") != std::string::npos) {
                    isVertex = false;
                    isFragment = true;
                    continue;
                }

                // For both vertex and fragment sections
                std::stringstream* currentStream = nullptr;
                if (isVertex) {
                    currentStream = &vertexStream;
                }
                else if (isFragment) {
                    currentStream = &fragmentStream;
                }
                else {
                    // Not in a shader section, skip this line
                    CCPrint(PrintManager::CHANNEL_WARN, "Ignoring line outside shader section: %s", line.c_str());
                    continue;
                }

                // Check for include directives
                std::string includePath;
                if (ExtractIncludePath(line, includePath))
                {
                    // Process the include and get its content
                    std::string includeContent = ProcessInclude(includePath);

                    // Add the included content to the current stream
                    *currentStream << "// Begin include: " << includePath << '\n';
                    *currentStream << includeContent;
                    *currentStream << "// End include: " << includePath << '\n';
                }
                else
                {
                    // Regular line, add it to the current stream
                    *currentStream << line << '\n';
                }
            }

            shaderDef->vertexShaderText = vertexStream.str();
            shaderDef->fragmentShaderText = fragmentStream.str();
            shaderDef->lastWriteTime = PlatformFileSystem::Get()->GetLastWriteTime(shaderDef->shaderFilePath.c_str());
        }
        else
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to open shader file: %s", shaderDef->shaderFilePath.c_str());
        }
    }

    void ShaderManager::CompileShader(const std::string& shaderName)
    {
        auto it = shaderMap.find(shaderName);
        if (it != shaderMap.end())
        {
            CompileShader(it->second);
        }
        else
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Cannot compile shader: %s - shader not found", shaderName.c_str());
        }
    }

    void ShaderManager::CompileShader(ShaderDefinition* shaderDef)
    {
        ParseShader(shaderDef);

        InjectShaderPreamble(shaderDef->vertexShaderText,   false);
        InjectShaderPreamble(shaderDef->fragmentShaderText, true);

        // Destroy any previous compiled program (hot-reload path)
        if (shaderDef->shaderHandle.IsValid())
        {
            Gfx::RenderApi::Get()->DestroyShader(shaderDef->shaderHandle);
            shaderDef->shaderHandle = Gfx::ShaderHandle();
        }

        Gfx::ShaderDescription description;
        description.vertexSource   = shaderDef->vertexShaderText.c_str();
        description.fragmentSource = shaderDef->fragmentShaderText.c_str();
        description.debugName      = shaderDef->shaderFilePath.c_str();

        shaderDef->shaderHandle = Gfx::RenderApi::Get()->CreateShader(description);
        shaderDef->isCompiled   = shaderDef->shaderHandle.IsValid();

        // Material writes custom parameters into the shader-declared
        // CustomParams block by std140 offset.
        shaderDef->customParamLayout.ParseFromFragmentSource(shaderDef->fragmentShaderText);
    }

    bool ShaderManager::HasFileChanged(const std::string& shaderName)
    {
        auto it = shaderMap.find(shaderName);
        if (it != shaderMap.end())
        {
            ShaderDefinition* shaderDef = it->second;
            time_t lastWriteTime = PlatformFileSystem::Get()->GetLastWriteTime(shaderDef->shaderFilePath.c_str());
            return lastWriteTime > shaderDef->lastWriteTime;
        }
        return false;
    }

    bool ShaderManager::IsShaderCompiled(const std::string& shaderName)
    {
        auto it = shaderMap.find(shaderName);
        if (it != shaderMap.end())
        {
            return it->second->isCompiled;
        }
        return false;
    }
}