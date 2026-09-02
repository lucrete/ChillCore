#include "ObjLoader.h"
#include "TangentGenerator.h"
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include "CCFile.h"
#include "PrintManager.h"
#include "MaterialManager.h"
#include "TextureManager.h"
#include "PlatformFileSystem.h"

namespace CC
{
    // ========================
    // Public Methods
    // ========================

    RenderableMesh* ObjLoader::LoadObj(const std::string& filePath, Material* material)
    {
        std::vector<float> positions;
        std::vector<float> texCoords;
        std::vector<float> normals;
        std::vector<Face> faces;
        std::string mtlFileName;

        if (!ParseObjFile(filePath, positions, texCoords, normals, faces, mtlFileName))
        {
            return nullptr;
        }

        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        ProcessMeshData(positions, texCoords, normals, faces, vertices, indices);

        RenderableMesh* mesh = CreateMesh(material, vertices, indices);

        return mesh;
    }

    RenderableMesh* ObjLoader::LoadObj(const std::string& filePath)
    {
        std::vector<float> positions;
        std::vector<float> texCoords;
        std::vector<float> normals;
        std::vector<Face> faces;
        std::string mtlFileName;

        if (!ParseObjFile(filePath, positions, texCoords, normals, faces, mtlFileName))
        {
            return nullptr;
        }

        // Parse MTL file if present
        std::vector<MtlMaterial> mtlMaterials;
        Material* material = nullptr;

        if (!mtlFileName.empty())
        {
            std::string objDirectory = CCFile::GetDirectoryFromPath(filePath);
            std::string mtlFilePath = objDirectory + mtlFileName;

            if (ParseMtlFile(mtlFilePath, mtlMaterials))
            {
                // Find first material with a texture, or use first material
                MtlMaterial* selectedMaterial = nullptr;
                for (auto& mtlMat : mtlMaterials)
                {
                    if (!mtlMat.diffuseTexture.empty())
                    {
                        selectedMaterial = &mtlMat;
                        break;
                    }
                }

                if (selectedMaterial == nullptr && !mtlMaterials.empty())
                {
                    selectedMaterial = &mtlMaterials[0];
                }

                if (selectedMaterial != nullptr)
                {
                    // Generate unique material name based on OBJ filename
                    std::string baseName = CCFile::GetFilenameNoExtension(filePath);
                    std::string materialName = "ObjMat_" + baseName;

                    // Register texture if present
                    std::string textureName = "";
                    if (!selectedMaterial->diffuseTexture.empty())
                    {
                        textureName = selectedMaterial->diffuseTexture;
                        std::string textureFilePath = objDirectory + selectedMaterial->diffuseTexture;

                        // Diffuse is colour, so it is decoded on sample.
                        if (!TextureManager::Get()->HasTexture(textureName, TextureColorSpace::Srgb))
                        {
                            TextureManager::Get()->AddTextureWithFullPath(textureName, textureFilePath, TextureColorSpace::Srgb);
                            CCPrint(PrintManager::CHANNEL_ALWAYS, "ObjLoader: Loaded texture '%s'", textureName.c_str());
                        }
                    }

                    // Create material if it doesn't exist
                    if (!MaterialManager::Get()->HasMaterial(materialName))
                    {
                        MaterialManager::Get()->CreateMaterial(
                            materialName,
                            "LitColour",
                            textureName,
                            selectedMaterial->diffuseColor,
                            Vector2(1.0f, 1.0f)
                        );
                        CCPrint(PrintManager::CHANNEL_ALWAYS, "ObjLoader: Created material '%s'", materialName.c_str());
                    }

                    material = MaterialManager::Get()->GetMaterial(materialName);
                }
            }
        }

        // Fallback to default material if none was created
        if (material == nullptr)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "ObjLoader: No MTL material found, using DefaultBasic");
            material = MaterialManager::Get()->GetMaterial("DefaultBasic");
        }

        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        ProcessMeshData(positions, texCoords, normals, faces, vertices, indices);

        RenderableMesh* mesh = CreateMesh(material, vertices, indices);

        return mesh;
    }

    // ========================
    // Private Methods
    // ========================

    bool ObjLoader::ParseObjFile(const std::string& filePath,
        std::vector<float>& positions,
        std::vector<float>& texCoords,
        std::vector<float>& normals,
        std::vector<Face>& faces,
        std::string& mtlFileName)
    {
        bool succeeded = false;

        std::string objContents;
        if (!PlatformFileSystem::Get()->ReadFileText(filePath.c_str(), objContents))
        {
            std::cerr << "Failed to open OBJ file: " << filePath << std::endl;
        }
        else
        {
            mtlFileName = "";

            std::istringstream file(objContents);
            std::string line;
            while (std::getline(file, line))
            {
                std::istringstream iss(line);
                std::string token;
                iss >> token;

                if (token == "mtllib")
                {
                    // Read the rest of the line as filename (may contain spaces)
                    std::getline(iss >> std::ws, mtlFileName);
                }
                else if (token == "v")
                {
                    float x, y, z;
                    iss >> x >> y >> z;
                    positions.push_back(x);
                    positions.push_back(y);
                    positions.push_back(z);
                }
                else if (token == "vt")
                {
                    float u, v;
                    iss >> u >> v;
                    texCoords.push_back(u);
                    texCoords.push_back(v);
                }
                else if (token == "vn")
                {
                    float nx, ny, nz;
                    iss >> nx >> ny >> nz;
                    normals.push_back(nx);
                    normals.push_back(ny);
                    normals.push_back(nz);
                }
                else if (token == "f")
                {
                    Face face;

                    for (int i = 0; i < 3; i++)
                    {
                        std::string vertexData;
                        iss >> vertexData;

                        // Parse the vertex indices (format: position/texcoord/normal)
                        std::replace(vertexData.begin(), vertexData.end(), '/', ' ');
                        std::istringstream viss(vertexData);

                        unsigned int posIndex, texIndex, normIndex;
                        viss >> posIndex >> texIndex >> normIndex;

                        // OBJ indices are 1-based, convert to 0-based
                        face.positionIndices[i] = posIndex - 1;
                        face.texCoordIndices[i] = texIndex - 1;
                        face.normalIndices[i] = normIndex - 1;
                    }

                    faces.push_back(face);
                }
            }

            succeeded = !positions.empty() && !faces.empty();
        }

        return succeeded;
    }

    bool ObjLoader::ParseMtlFile(const std::string& filePath,
        std::vector<MtlMaterial>& materials)
    {
        bool succeeded = false;

        std::string mtlContents;
        if (!PlatformFileSystem::Get()->ReadFileText(filePath.c_str(), mtlContents))
        {
            CCPrint(PrintManager::CHANNEL_WARN, "ObjLoader: Failed to open MTL file: %s", filePath.c_str());
        }
        else
        {
            MtlMaterial* currentMaterial = nullptr;

            std::istringstream file(mtlContents);
            std::string line;
            while (std::getline(file, line))
            {
                std::istringstream iss(line);
                std::string token;
                iss >> token;

                if (token == "newmtl")
                {
                    materials.push_back(MtlMaterial());
                    currentMaterial = &materials.back();
                    std::getline(iss >> std::ws, currentMaterial->name);
                }
                else if (token == "Kd" && currentMaterial != nullptr)
                {
                    float r, g, b;
                    iss >> r >> g >> b;
                    currentMaterial->diffuseColor = Vector3(r, g, b);
                }
                else if (token == "map_Kd" && currentMaterial != nullptr)
                {
                    std::getline(iss >> std::ws, currentMaterial->diffuseTexture);
                }
            }

            succeeded = !materials.empty();
        }

        return succeeded;
    }

    RenderableMesh* ObjLoader::CreateMesh(Material* material,
        const std::vector<float>& vertices,
        const std::vector<unsigned int>& indices)
    {
        RenderableMesh* mesh = new RenderableMesh(material);

        // Tangents exist only to build the TBN frame for normal mapping, and
        // the PBR shader is the only one declaring the attribute. A mesh on
        // any other shader keeps the narrower vertex.
        if (material != nullptr && material->GetShaderName() == "Pbr")
        {
            std::vector<float> tangentVertices;
            BuildTangentVertices(vertices, indices, tangentVertices);
            mesh->InitializeWithTangents(tangentVertices.data(), indices.data(),
                (int)(tangentVertices.size() * sizeof(float)),
                (int)(indices.size() * sizeof(unsigned int)));
        }
        else
        {
            mesh->Initialize(vertices.data(), indices.data(),
                (int)(vertices.size() * sizeof(float)),
                (int)(indices.size() * sizeof(unsigned int)));
        }

        mesh->SetIndexCount((int)indices.size());

        return mesh;
    }

    void ObjLoader::BuildTangentVertices(const std::vector<float>& vertices,
        const std::vector<unsigned int>& indices,
        std::vector<float>& tangentVertices)
    {
        static const int SOURCE_FLOATS_PER_VERTEX = 8;
        static const int RESULT_FLOATS_PER_VERTEX = 12;

        size_t vertexCount = vertices.size() / SOURCE_FLOATS_PER_VERTEX;

        std::vector<float> positions;
        std::vector<float> texCoords;
        std::vector<float> normals;
        positions.reserve(vertexCount * 3);
        texCoords.reserve(vertexCount * 2);
        normals.reserve(vertexCount * 3);

        for (size_t i = 0; i < vertexCount; i++)
        {
            const float* vertex = &vertices[i * SOURCE_FLOATS_PER_VERTEX];
            positions.push_back(vertex[0]);
            positions.push_back(vertex[1]);
            positions.push_back(vertex[2]);
            texCoords.push_back(vertex[3]);
            texCoords.push_back(vertex[4]);
            normals.push_back(vertex[5]);
            normals.push_back(vertex[6]);
            normals.push_back(vertex[7]);
        }

        std::vector<float> tangents;
        TangentGenerator::Compute(positions, texCoords, normals, indices, tangents);

        tangentVertices.clear();
        tangentVertices.reserve(vertexCount * RESULT_FLOATS_PER_VERTEX);

        for (size_t i = 0; i < vertexCount; i++)
        {
            const float* vertex = &vertices[i * SOURCE_FLOATS_PER_VERTEX];
            for (int component = 0; component < SOURCE_FLOATS_PER_VERTEX; component++)
            {
                tangentVertices.push_back(vertex[component]);
            }

            tangentVertices.push_back(tangents[i * 4]);
            tangentVertices.push_back(tangents[i * 4 + 1]);
            tangentVertices.push_back(tangents[i * 4 + 2]);
            tangentVertices.push_back(tangents[i * 4 + 3]);
        }
    }

    void ObjLoader::ProcessMeshData(const std::vector<float>& positions,
        const std::vector<float>& texCoords,
        const std::vector<float>& normals,
        const std::vector<Face>& faces,
        std::vector<float>& vertices,
        std::vector<unsigned int>& indices)
    {
        // Create a map to track unique vertices
        std::unordered_map<std::string, unsigned int> uniqueVertices;

        for (size_t i = 0; i < faces.size(); i++)
        {
            for (int j = 0; j < 3; j++)
            {
                // Get indices for this vertex
                unsigned int posIndex = faces[i].positionIndices[j] * 3;
                unsigned int texIndex = faces[i].texCoordIndices[j] * 2;
                unsigned int normIndex = faces[i].normalIndices[j] * 3;

                // Create a key for this unique vertex
                std::stringstream ss;
                ss << posIndex << ":" << texIndex << ":" << normIndex;
                std::string key = ss.str();

                // Check if we've seen this vertex before
                if (uniqueVertices.find(key) == uniqueVertices.end())
                {
                    // Add a new vertex
                    uniqueVertices[key] = (unsigned int)(vertices.size() / 8);  // 8 floats per vertex

                    // Position
                    vertices.push_back(positions[posIndex]);
                    vertices.push_back(positions[posIndex + 1]);
                    vertices.push_back(positions[posIndex + 2]);

                    // Texture coordinates
                    if (!texCoords.empty())
                    {
                        vertices.push_back(texCoords[texIndex]);
                        vertices.push_back(texCoords[texIndex + 1]);
                    }
                    else
                    {
                        // Default texture coordinates
                        vertices.push_back(0.0f);
                        vertices.push_back(0.0f);
                    }

                    // Normal
                    if (!normals.empty())
                    {
                        vertices.push_back(normals[normIndex]);
                        vertices.push_back(normals[normIndex + 1]);
                        vertices.push_back(normals[normIndex + 2]);
                    }
                    else
                    {
                        // Default normal
                        vertices.push_back(0.0f);
                        vertices.push_back(1.0f);
                        vertices.push_back(0.0f);
                    }
                }

                // Add the index to our index buffer
                indices.push_back(uniqueVertices[key]);
            }
        }


        if (texCoords.empty())
        {
            CCPrint(PrintManager::CHANNEL_WARN, "ObjLoad: no tex coords");
        }

        if (normals.empty())
        {
            CCPrint(PrintManager::CHANNEL_WARN, "ObjLoad: no normals");
        }
    }

}
