#ifndef OBJLOADER_H
#define OBJLOADER_H

#include <string>
#include <vector>
#include "RenderableMesh.h"
#include "Material.h"
#include "CCVector3.h"

namespace CC
{
    class ObjLoader
    {
    public:
        // Load OBJ with explicit material (original behavior)
        static RenderableMesh* LoadObj(const std::string& filePath, Material* material);

        // Load OBJ with auto-created material from MTL file
        static RenderableMesh* LoadObj(const std::string& filePath);

    private:
        struct Vertex
        {
            float position[3];
            float texCoord[2];
            float normal[3];
        };

        struct Face
        {
            unsigned int positionIndices[3];
            unsigned int texCoordIndices[3];
            unsigned int normalIndices[3];
        };

        struct MtlMaterial
        {
            std::string name;
            Vector3 diffuseColor = Vector3(1.0f, 1.0f, 1.0f);
            std::string diffuseTexture;
        };

        static bool ParseObjFile(const std::string& filePath,
            std::vector<float>& positions,
            std::vector<float>& texCoords,
            std::vector<float>& normals,
            std::vector<Face>& faces,
            std::string& mtlFileName);

        static bool ParseMtlFile(const std::string& filePath,
            std::vector<MtlMaterial>& materials);

        static void ProcessMeshData(const std::vector<float>& positions,
            const std::vector<float>& texCoords,
            const std::vector<float>& normals,
            const std::vector<Face>& faces,
            std::vector<float>& vertices,
            std::vector<unsigned int>& indices);

    };
}

#endif // OBJLOADER_H