#define _CRT_SECURE_NO_WARNINGS
#define CGLTF_IMPLEMENTATION
#include "GltfLoader.h"
#include "TangentGenerator.h"
#include <cgltf.h>
#include <sstream>
#include <unordered_map>
#include <cmath>
#include "CCFile.h"
#include "PrintManager.h"
#include "MaterialManager.h"
#include "TextureManager.h"
#include "SceneObject.h"
#include "PlatformFileSystem.h"

namespace CC
{
    // ========================
    // Helper Functions
    // ========================

    // colorSpace follows the glTF slot: base colour and emissive are colour
    // and are decoded on sample; normal, metallic-roughness and occlusion
    // carry measurements and stay linear.
    static bool ExtractAndRegisterTexture(cgltf_texture_view* textureView, const std::string& directory,
        const std::string& textureName, TextureColorSpace colorSpace)
    {
        if (textureView == nullptr || textureView->texture == nullptr)
        {
            return false;
        }

        cgltf_image* image = textureView->texture->image;
        if (image == nullptr)
        {
            return false;
        }

        // Skip if already loaded
        if (TextureManager::Get()->HasTexture(textureName, colorSpace))
        {
            return true;
        }

        // Check for buffer_view (embedded in GLB)
        if (image->buffer_view != nullptr)
        {
            const uint8_t* imageData = cgltf_buffer_view_data(image->buffer_view);
            if (imageData != nullptr)
            {
                int dataSize = (int)image->buffer_view->size;
                TextureManager::Get()->AddTextureFromMemory(textureName, imageData, dataSize, colorSpace);
                CCPrint(PrintManager::CHANNEL_ALWAYS, "GltfLoader: Loaded embedded texture '%s' (%d bytes)",
                    textureName.c_str(), dataSize);
                return true;
            }
        }

        // Check for external URI (file path)
        if (image->uri != nullptr && strlen(image->uri) > 0)
        {
            // Skip data URIs (embedded base64)
            if (strncmp(image->uri, "data:", 5) == 0)
            {
                CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: Data URI textures not yet supported");
                return false;
            }

            std::string texturePath = directory + image->uri;
            TextureManager::Get()->AddTextureWithFullPath(textureName, texturePath, colorSpace);
            CCPrint(PrintManager::CHANNEL_ALWAYS, "GltfLoader: Loaded texture '%s'", textureName.c_str());
            return true;
        }

        return false;
    }

    // ========================
    // Public Methods
    // ========================

    SceneObject* GltfLoader::LoadGltf(const std::string& filePath)
    {
        cgltf_options options = {};
        cgltf_data* data = nullptr;

        // Read via PlatformFileSystem so the APK asset path works on
        // Android. The byte buffer must stay alive across cgltf_parse +
        // cgltf_load_buffers + the data-traversal that follows because
        // cgltf_data references the input bytes (notably .glb's bin chunk).
        std::vector<uint8_t> fileBytes;
        if (!PlatformFileSystem::Get()->ReadFileBinary(filePath.c_str(), fileBytes))
        {
            CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: Failed to read file '%s'", filePath.c_str());
            return nullptr;
        }

        cgltf_result result = cgltf_parse(&options, fileBytes.data(), fileBytes.size(), &data);
        if (result != cgltf_result_success)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: Failed to parse file '%s'", filePath.c_str());
            return nullptr;
        }

        result = cgltf_load_buffers(&options, data, filePath.c_str());
        if (result != cgltf_result_success)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: Failed to load buffers for '%s'", filePath.c_str());
            cgltf_free(data);
            return nullptr;
        }

        std::string directory = CCFile::GetDirectoryFromPath(filePath);
        std::string baseName = CCFile::GetFilenameNoExtension(filePath);

        SceneObject* rootObject = new SceneObject(baseName);

        CCPrint(PrintManager::CHANNEL_ALWAYS, "GltfLoader: Loading '%s' with %zu meshes, %zu nodes",
            baseName.c_str(), data->meshes_count, data->nodes_count);

        // Traverse scene nodes to preserve transforms
        int meshIndex = 0;
        if (data->scenes_count > 0)
        {
            cgltf_scene* scene = &data->scenes[data->scene ? (data->scene - data->scenes) : 0];
            for (size_t n = 0; n < scene->nodes_count; n++)
            {
                SceneObject* nodeObject = ProcessNode(scene->nodes[n], data, baseName, directory, meshIndex);
                if (nodeObject != nullptr)
                {
                    rootObject->AddChild(nodeObject);
                }
            }
        }

        cgltf_free(data);

        CCPrint(PrintManager::CHANNEL_ALWAYS, "GltfLoader: Loaded %d primitives from '%s'", meshIndex, baseName.c_str());
        return rootObject;
    }

    SceneObject* GltfLoader::ProcessNode(cgltf_node* node, cgltf_data* data,
        const std::string& baseName, const std::string& directory, int& meshIndex)
    {
        std::string nodeName = node->name ? node->name : (baseName + "_node_" + std::to_string(meshIndex));
        SceneObject* nodeObject = new SceneObject(nodeName);

        // Apply node transform (TRS fields are always populated with defaults by cgltf)
        if (node->has_translation)
        {
            nodeObject->GetTransform().SetPosition(Vector3(
                node->translation[0], node->translation[1], node->translation[2]));
        }
        if (node->has_rotation)
        {
            nodeObject->GetTransform().SetRotationQuaternion(Quaternion(
                node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]));
        }
        if (node->has_scale)
        {
            nodeObject->GetTransform().SetScale(Vector3(
                node->scale[0], node->scale[1], node->scale[2]));
        }

        // Process mesh primitives if this node has a mesh
        if (node->mesh != nullptr)
        {
            cgltf_mesh* mesh = node->mesh;

            for (size_t p = 0; p < mesh->primitives_count; p++)
            {
                cgltf_primitive* primitive = &mesh->primitives[p];

                if (primitive->type != cgltf_primitive_type_triangles)
                {
                    CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: Skipping non-triangle primitive");
                    continue;
                }

                // Extract vertex data
                std::vector<float> positions;
                std::vector<float> texCoords;
                std::vector<float> normals;
                std::vector<float> tangents;
                std::vector<unsigned int> indices;

                cgltf_accessor* posAccessor = nullptr;
                cgltf_accessor* texCoordAccessor = nullptr;
                cgltf_accessor* normalAccessor = nullptr;
                cgltf_accessor* tangentAccessor = nullptr;

                for (size_t a = 0; a < primitive->attributes_count; a++)
                {
                    cgltf_attribute* attr = &primitive->attributes[a];
                    switch (attr->type)
                    {
                    case cgltf_attribute_type_position:
                        posAccessor = attr->data;
                        break;
                    case cgltf_attribute_type_texcoord:
                        if (attr->index == 0)
                        {
                            texCoordAccessor = attr->data;
                        }
                        break;
                    case cgltf_attribute_type_normal:
                        normalAccessor = attr->data;
                        break;
                    case cgltf_attribute_type_tangent:
                        tangentAccessor = attr->data;
                        break;
                    default:
                        break;
                    }
                }

                if (posAccessor == nullptr)
                {
                    CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: Primitive missing position data");
                    continue;
                }

                size_t vertexCount = posAccessor->count;

                // Read positions
                positions.resize(vertexCount * 3);
                for (size_t v = 0; v < vertexCount; v++)
                {
                    cgltf_accessor_read_float(posAccessor, v, &positions[v * 3], 3);
                }

                // Read texture coordinates
                texCoords.resize(vertexCount * 2, 0.0f);
                if (texCoordAccessor != nullptr)
                {
                    for (size_t v = 0; v < vertexCount; v++)
                    {
                        cgltf_accessor_read_float(texCoordAccessor, v, &texCoords[v * 2], 2);
                    }
                }

                // Read normals
                normals.resize(vertexCount * 3, 0.0f);
                if (normalAccessor != nullptr)
                {
                    for (size_t v = 0; v < vertexCount; v++)
                    {
                        cgltf_accessor_read_float(normalAccessor, v, &normals[v * 3], 3);
                    }
                }
                else
                {
                    // Default normals pointing up
                    for (size_t v = 0; v < vertexCount; v++)
                    {
                        normals[v * 3 + 1] = 1.0f;
                    }
                }

                // Read indices
                if (primitive->indices != nullptr)
                {
                    indices.resize(primitive->indices->count);
                    for (size_t i = 0; i < primitive->indices->count; i++)
                    {
                        indices[i] = (unsigned int)cgltf_accessor_read_index(primitive->indices, i);
                    }
                }
                else
                {
                    // Generate sequential indices
                    indices.resize(vertexCount);
                    for (size_t i = 0; i < vertexCount; i++)
                    {
                        indices[i] = (unsigned int)i;
                    }
                }

                // Read or compute tangents
                if (tangentAccessor != nullptr)
                {
                    tangents.resize(vertexCount * 4);
                    for (size_t v = 0; v < vertexCount; v++)
                    {
                        cgltf_accessor_read_float(tangentAccessor, v, &tangents[v * 4], 4);
                    }
                }
                else
                {
                    TangentGenerator::Compute(positions, texCoords, normals, indices, tangents);
                }

                // Extract PBR material data
                PbrMaterialData pbrData;
                pbrData.name = baseName + "_Mat_" + std::to_string(meshIndex);

                if (primitive->material != nullptr)
                {
                    cgltf_material* mat = primitive->material;

                    if (mat->has_pbr_metallic_roughness)
                    {
                        cgltf_pbr_metallic_roughness* pbr = &mat->pbr_metallic_roughness;

                        pbrData.baseColorFactor = Vector3(
                            pbr->base_color_factor[0],
                            pbr->base_color_factor[1],
                            pbr->base_color_factor[2]
                        );
                        pbrData.metallicFactor = pbr->metallic_factor;
                        pbrData.roughnessFactor = pbr->roughness_factor;

                        // Base color texture
                        if (pbr->base_color_texture.texture != nullptr)
                        {
                            std::string texName = pbrData.name + "_baseColor";
                            if (ExtractAndRegisterTexture(&pbr->base_color_texture, directory, texName, TextureColorSpace::Srgb))
                            {
                                pbrData.baseColorTexture = texName;
                            }
                        }

                        // Metallic-roughness texture
                        if (pbr->metallic_roughness_texture.texture != nullptr)
                        {
                            std::string texName = pbrData.name + "_metallicRoughness";
                            if (ExtractAndRegisterTexture(&pbr->metallic_roughness_texture, directory, texName, TextureColorSpace::Linear))
                            {
                                pbrData.metallicRoughnessTexture = texName;
                            }
                        }
                    }

                    // Normal texture
                    if (mat->normal_texture.texture != nullptr)
                    {
                        std::string texName = pbrData.name + "_normal";
                        if (ExtractAndRegisterTexture(&mat->normal_texture, directory, texName, TextureColorSpace::Linear))
                        {
                            pbrData.normalTexture = texName;
                        }
                    }

                    // Occlusion texture
                    if (mat->occlusion_texture.texture != nullptr)
                    {
                        std::string texName = pbrData.name + "_occlusion";
                        if (ExtractAndRegisterTexture(&mat->occlusion_texture, directory, texName, TextureColorSpace::Linear))
                        {
                            pbrData.occlusionTexture = texName;
                        }
                    }

                    // Emissive texture
                    if (mat->emissive_texture.texture != nullptr)
                    {
                        std::string texName = pbrData.name + "_emissive";
                        if (ExtractAndRegisterTexture(&mat->emissive_texture, directory, texName, TextureColorSpace::Srgb))
                        {
                            pbrData.emissiveTexture = texName;
                        }
                    }

                    pbrData.emissiveFactor = Vector3(
                        mat->emissive_factor[0],
                        mat->emissive_factor[1],
                        mat->emissive_factor[2]
                    );

                    if (mat->alpha_mode == cgltf_alpha_mode_blend)
                    {
                        pbrData.alphaMode = AlphaBlendMode::Blend;
                        if (mat->has_pbr_metallic_roughness)
                        {
                            pbrData.opacity = mat->pbr_metallic_roughness.base_color_factor[3];
                        }
                    }
                    else if (mat->alpha_mode == cgltf_alpha_mode_mask)
                    {
                        pbrData.alphaMode = AlphaBlendMode::Mask;
                        pbrData.alphaCutoff = mat->alpha_cutoff;
                    }
                }

                // Create PBR material
                Material* material = nullptr;
                if (!MaterialManager::Get()->HasMaterial(pbrData.name))
                {
                    MaterialManager::Get()->CreatePbrMaterial(
                        pbrData.name,
                        pbrData.baseColorTexture,
                        pbrData.metallicRoughnessTexture,
                        pbrData.normalTexture,
                        pbrData.occlusionTexture,
                        pbrData.emissiveTexture,
                        pbrData.baseColorFactor,
                        pbrData.metallicFactor,
                        pbrData.roughnessFactor,
                        pbrData.emissiveFactor,
                        pbrData.opacity
                    );
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "GltfLoader: Created PBR material '%s'", pbrData.name.c_str());
                }
                material = MaterialManager::Get()->GetMaterial(pbrData.name);
                material->SetAlphaMode(pbrData.alphaMode);
                material->SetAlphaCutoff(pbrData.alphaCutoff);

                // Build interleaved vertex data (12 floats: pos3 + uv2 + normal3 + tangent4)
                std::vector<float> vertices;
                vertices.reserve(vertexCount * 12);
                for (size_t v = 0; v < vertexCount; v++)
                {
                    vertices.push_back(positions[v * 3]);
                    vertices.push_back(positions[v * 3 + 1]);
                    vertices.push_back(positions[v * 3 + 2]);
                    vertices.push_back(texCoords[v * 2]);
                    vertices.push_back(texCoords[v * 2 + 1]);
                    vertices.push_back(normals[v * 3]);
                    vertices.push_back(normals[v * 3 + 1]);
                    vertices.push_back(normals[v * 3 + 2]);
                    vertices.push_back(tangents[v * 4]);
                    vertices.push_back(tangents[v * 4 + 1]);
                    vertices.push_back(tangents[v * 4 + 2]);
                    vertices.push_back(tangents[v * 4 + 3]);
                }

                // Create RenderableMesh
                RenderableMesh* renderableMesh = new RenderableMesh(material);
                renderableMesh->InitializeWithTangents(
                    vertices.data(),
                    indices.data(),
                    (int)(vertices.size() * sizeof(float)),
                    (int)(indices.size() * sizeof(unsigned int))
                );
                renderableMesh->SetIndexCount((int)indices.size());

                // For single-primitive meshes, add directly to the node object.
                // For multi-primitive meshes, create child objects.
                if (mesh->primitives_count == 1)
                {
                    nodeObject->AddComponent(renderableMesh);
                }
                else
                {
                    std::string primName = nodeName + "_prim_" + std::to_string(p);
                    SceneObject* primObject = new SceneObject(primName);
                    primObject->AddComponent(renderableMesh);
                    nodeObject->AddChild(primObject);
                }

                meshIndex++;
            }
        }

        // Process child nodes recursively
        for (size_t c = 0; c < node->children_count; c++)
        {
            SceneObject* childObject = ProcessNode(node->children[c], data, baseName, directory, meshIndex);
            if (childObject != nullptr)
            {
                nodeObject->AddChild(childObject);
            }
        }

        return nodeObject;
    }

    RenderableMesh* GltfLoader::LoadGltfMesh(const std::string& filePath, Material* material)
    {
        cgltf_options options = {};
        cgltf_data* data = nullptr;

        std::vector<uint8_t> fileBytes;
        if (!PlatformFileSystem::Get()->ReadFileBinary(filePath.c_str(), fileBytes))
        {
            CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: Failed to read file '%s'", filePath.c_str());
            return nullptr;
        }

        cgltf_result result = cgltf_parse(&options, fileBytes.data(), fileBytes.size(), &data);
        if (result != cgltf_result_success)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: Failed to parse file '%s'", filePath.c_str());
            return nullptr;
        }

        result = cgltf_load_buffers(&options, data, filePath.c_str());
        if (result != cgltf_result_success)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: Failed to load buffers for '%s'", filePath.c_str());
            cgltf_free(data);
            return nullptr;
        }

        if (data->meshes_count == 0)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: No meshes found in '%s'", filePath.c_str());
            cgltf_free(data);
            return nullptr;
        }

        // Get first primitive from first mesh
        cgltf_mesh* mesh = &data->meshes[0];
        if (mesh->primitives_count == 0)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: No primitives in first mesh");
            cgltf_free(data);
            return nullptr;
        }

        cgltf_primitive* primitive = &mesh->primitives[0];

        // Extract vertex data
        std::vector<float> positions;
        std::vector<float> texCoords;
        std::vector<float> normals;
        std::vector<unsigned int> indices;

        cgltf_accessor* posAccessor = nullptr;
        cgltf_accessor* texCoordAccessor = nullptr;
        cgltf_accessor* normalAccessor = nullptr;

        for (size_t a = 0; a < primitive->attributes_count; a++)
        {
            cgltf_attribute* attr = &primitive->attributes[a];
            switch (attr->type)
            {
            case cgltf_attribute_type_position:
                posAccessor = attr->data;
                break;
            case cgltf_attribute_type_texcoord:
                if (attr->index == 0)
                {
                    texCoordAccessor = attr->data;
                }
                break;
            case cgltf_attribute_type_normal:
                normalAccessor = attr->data;
                break;
            default:
                break;
            }
        }

        if (posAccessor == nullptr)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "GltfLoader: Primitive missing position data");
            cgltf_free(data);
            return nullptr;
        }

        size_t vertexCount = posAccessor->count;

        // Read positions
        positions.resize(vertexCount * 3);
        for (size_t v = 0; v < vertexCount; v++)
        {
            cgltf_accessor_read_float(posAccessor, v, &positions[v * 3], 3);
        }

        // Read texture coordinates
        texCoords.resize(vertexCount * 2, 0.0f);
        if (texCoordAccessor != nullptr)
        {
            for (size_t v = 0; v < vertexCount; v++)
            {
                cgltf_accessor_read_float(texCoordAccessor, v, &texCoords[v * 2], 2);
            }
        }

        // Read normals
        normals.resize(vertexCount * 3, 0.0f);
        if (normalAccessor != nullptr)
        {
            for (size_t v = 0; v < vertexCount; v++)
            {
                cgltf_accessor_read_float(normalAccessor, v, &normals[v * 3], 3);
            }
        }
        else
        {
            for (size_t v = 0; v < vertexCount; v++)
            {
                normals[v * 3 + 1] = 1.0f;
            }
        }

        // Read indices
        if (primitive->indices != nullptr)
        {
            indices.resize(primitive->indices->count);
            for (size_t i = 0; i < primitive->indices->count; i++)
            {
                indices[i] = (unsigned int)cgltf_accessor_read_index(primitive->indices, i);
            }
        }
        else
        {
            indices.resize(vertexCount);
            for (size_t i = 0; i < vertexCount; i++)
            {
                indices[i] = (unsigned int)i;
            }
        }

        // Build interleaved vertex data (8 floats: pos3 + uv2 + normal3)
        std::vector<float> vertices;
        vertices.reserve(vertexCount * 8);
        for (size_t v = 0; v < vertexCount; v++)
        {
            vertices.push_back(positions[v * 3]);
            vertices.push_back(positions[v * 3 + 1]);
            vertices.push_back(positions[v * 3 + 2]);
            vertices.push_back(texCoords[v * 2]);
            vertices.push_back(texCoords[v * 2 + 1]);
            vertices.push_back(normals[v * 3]);
            vertices.push_back(normals[v * 3 + 1]);
            vertices.push_back(normals[v * 3 + 2]);
        }

        cgltf_free(data);

        // Create mesh
        RenderableMesh* renderableMesh = new RenderableMesh(material);
        renderableMesh->Initialize(
            vertices.data(),
            indices.data(),
            (int)(vertices.size() * sizeof(float)),
            (int)(indices.size() * sizeof(unsigned int))
        );
        renderableMesh->SetIndexCount((int)indices.size());

        return renderableMesh;
    }

    RenderableMesh* GltfLoader::LoadGltfMesh(const std::string& filePath)
    {
        Material* material = MaterialManager::Get()->GetMaterial("DefaultBasic");
        return LoadGltfMesh(filePath, material);
    }

}
