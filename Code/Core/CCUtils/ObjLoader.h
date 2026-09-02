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

        // Builds the mesh from an 8-float pos3+uv2+normal3 stream, widening
        // it to carry a tangent when the material's shader declares one.
        static RenderableMesh* CreateMesh(Material* material,
            const std::vector<float>& vertices,
            const std::vector<unsigned int>& indices);

        // OBJ carries no tangent, so it is derived from the UVs and appended,
        // giving the 12-float pos3+uv2+normal3+tangent4 stream.
        static void BuildTangentVertices(const std::vector<float>& vertices,
            const std::vector<unsigned int>& indices,
            std::vector<float>& tangentVertices);

        static void ProcessMeshData(const std::vector<float>& positions,
            const std::vector<float>& texCoords,
            const std::vector<float>& normals,
            const std::vector<Face>& faces,
            std::vector<float>& vertices,
            std::vector<unsigned int>& indices);

    };
}

#endif // OBJLOADER_H