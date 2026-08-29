#ifndef SHADERPARAMLAYOUT_H
#define SHADERPARAMLAYOUT_H

#include <string>

namespace CC
{
    enum class ShaderParamType
    {
        Float = 0,
        Int,
        Vector2,
        Vector3,
        Vector4,
        Mat4,
        Max
    };

    struct ShaderParam
    {
        std::string     name;
        ShaderParamType type        = ShaderParamType::Float;
        int             offsetBytes = 0;
        int             sizeBytes   = 0;
    };

    // ========================
    // ShaderParamLayout
    // ========================
    //
    // std140 layout of the per-material parameter block a shader author
    // declares for custom parameters that no standard uniform tier covers —
    // procedural-art controls, the fade colour, the fullscreen aspect ratio.
    //
    // The block is declared in the shader as an unnamed interface block so
    // its members stay in global scope and read like plain uniforms:
    //
    //     layout(std140, binding = 5) uniform MaterialParams
    //     {
    //         float aspectRatio;
    //         vec2  center;
    //         float scale;
    //     };
    //
    // Offsets are computed here from the std140 alignment rules rather than
    // queried from the graphics API, so nothing in this path depends on a
    // backend that can reflect a program. Material writes values into the
    // block by offset and uploads it opaquely.
    //
    // Supported member types: float, int, vec2, vec3, vec4, mat4. Arrays,
    // structs, and matrices other than mat4 are rejected with a warning.
    class ShaderParamLayout
    {
    public:
        static constexpr int MAX_PARAMS            = 16;
        static constexpr int MAX_BLOCK_SIZE_BYTES  = 256;

        void Clear();
        void ParseFromFragmentSource(const std::string& fragmentSource);

        bool HasParams() const { return paramCount > 0; }
        int  GetBlockSizeBytes() const { return blockSizeBytes; }

        const ShaderParam* FindParam(const std::string& name) const;

    private:
        bool ExtractBlockBody(const std::string& source, std::string& outBlockBody) const;
        bool AddParam(const std::string& typeName, const std::string& paramName);

        static bool GetTypeInfo(const std::string& typeName, ShaderParamType& outType,
                                int& outAlignmentBytes, int& outSizeBytes);
        static std::string StripLineComments(const std::string& text);

        ShaderParam params[MAX_PARAMS];
        int         paramCount = 0;

        // Doubles as the packing cursor while parsing; rounded up to the
        // std140 block alignment of 16 bytes once every member is placed.
        int blockSizeBytes = 0;
    };
}

#endif // SHADERPARAMLAYOUT_H
