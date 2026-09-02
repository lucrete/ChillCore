#include "TangentGenerator.h"

#include "CCVector3.h"

namespace CC
{
    void TangentGenerator::Compute(const std::vector<float>& positions,
                                   const std::vector<float>& texCoords,
                                   const std::vector<float>& normals,
                                   const std::vector<unsigned int>& indices,
                                   std::vector<float>& tangents)
    {
        size_t vertexCount = positions.size() / 3;
        tangents.resize(vertexCount * 4, 0.0f);

        std::vector<Vector3> tan1(vertexCount, Vector3(0.0f, 0.0f, 0.0f));
        std::vector<Vector3> tan2(vertexCount, Vector3(0.0f, 0.0f, 0.0f));

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            unsigned int i1 = indices[i];
            unsigned int i2 = indices[i + 1];
            unsigned int i3 = indices[i + 2];

            Vector3 v1(positions[i1 * 3], positions[i1 * 3 + 1], positions[i1 * 3 + 2]);
            Vector3 v2(positions[i2 * 3], positions[i2 * 3 + 1], positions[i2 * 3 + 2]);
            Vector3 v3(positions[i3 * 3], positions[i3 * 3 + 1], positions[i3 * 3 + 2]);

            float u1 = texCoords[i1 * 2];
            float v1t = texCoords[i1 * 2 + 1];
            float u2 = texCoords[i2 * 2];
            float v2t = texCoords[i2 * 2 + 1];
            float u3 = texCoords[i3 * 2];
            float v3t = texCoords[i3 * 2 + 1];

            float x1 = v2.x - v1.x;
            float x2 = v3.x - v1.x;
            float y1 = v2.y - v1.y;
            float y2 = v3.y - v1.y;
            float z1 = v2.z - v1.z;
            float z2 = v3.z - v1.z;

            float s1 = u2 - u1;
            float s2 = u3 - u1;
            float t1 = v2t - v1t;
            float t2 = v3t - v1t;

            float r = 1.0f / (s1 * t2 - s2 * t1 + 0.0001f);

            Vector3 sdir(
                (t2 * x1 - t1 * x2) * r,
                (t2 * y1 - t1 * y2) * r,
                (t2 * z1 - t1 * z2) * r
            );

            Vector3 tdir(
                (s1 * x2 - s2 * x1) * r,
                (s1 * y2 - s2 * y1) * r,
                (s1 * z2 - s2 * z1) * r
            );

            tan1[i1] = tan1[i1] + sdir;
            tan1[i2] = tan1[i2] + sdir;
            tan1[i3] = tan1[i3] + sdir;

            tan2[i1] = tan2[i1] + tdir;
            tan2[i2] = tan2[i2] + tdir;
            tan2[i3] = tan2[i3] + tdir;
        }

        for (size_t i = 0; i < vertexCount; i++)
        {
            Vector3 n(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
            Vector3 t = tan1[i];

            // Gram-Schmidt orthogonalize
            Vector3 tangent = t - n * n.Dot(t);
            tangent.Normalize();

            // Calculate handedness
            float w = (n.Cross(t).Dot(tan2[i]) < 0.0f) ? -1.0f : 1.0f;

            tangents[i * 4] = tangent.x;
            tangents[i * 4 + 1] = tangent.y;
            tangents[i * 4 + 2] = tangent.z;
            tangents[i * 4 + 3] = w;
        }
    }
}
