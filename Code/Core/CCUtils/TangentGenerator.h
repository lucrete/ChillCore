#ifndef TANGENTGENERATOR_H
#define TANGENTGENERATOR_H

#include <vector>

namespace CC
{
    // ========================
    // TangentGenerator
    // ========================
    //
    // Per-vertex tangent frames for normal mapping, derived from the UV
    // parameterisation. Shared by every mesh loader: glTF uses it when an
    // asset ships without a TANGENT accessor, and the OBJ path uses it
    // always, since the format has no way to carry one.
    class TangentGenerator
    {
    public:
        // Accumulates per-triangle tangents, then Gram-Schmidt orthogonalises
        // each against its normal. The w component carries handedness, matching
        // the glTF TANGENT convention the shader's TBN construction expects.
        static void Compute(const std::vector<float>& positions,
                            const std::vector<float>& texCoords,
                            const std::vector<float>& normals,
                            const std::vector<unsigned int>& indices,
                            std::vector<float>& tangents);
    };
}

#endif // TANGENTGENERATOR_H
