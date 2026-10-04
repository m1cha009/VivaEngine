#include "Viva/MeshData.h"

#include <glm/common.hpp>

namespace Viva {

Bounds ComputeBounds(const MeshData& data)
{
    if (data.Vertices.empty())
        return {};
    // Start with the first vertex as both corners, then widen the box to take in each of the others.
    // glm::min and glm::max work on each of x, y and z separately.
    Bounds bounds { data.Vertices[0].Position, data.Vertices[0].Position };
    for (const Vertex& vertex : data.Vertices) {
        bounds.Min = glm::min(bounds.Min, vertex.Position);
        bounds.Max = glm::max(bounds.Max, vertex.Position);
    }
    return bounds;
}

} // namespace Viva
