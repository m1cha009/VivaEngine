#include "Viva/Primitives.h"

#include <array>

namespace Viva::Primitives {

namespace {

// Adds a square as two triangles that share the diagonal from corner 0 to corner 2. The corners
// must go counter-clockwise as seen from the side that should be the front.
void AddQuad(MeshData& mesh, const std::array<Vertex, 4>& corners)
{
    const auto first = static_cast<uint32_t>(mesh.Vertices.size());
    mesh.Vertices.insert(mesh.Vertices.end(), corners.begin(), corners.end());
    mesh.Indices.insert(mesh.Indices.end(), { first, first + 1, first + 2, first, first + 2, first + 3 });
}

constexpr glm::vec3 kWhite(1.0f);

} // namespace

MeshData Cube()
{
    // For each face: the direction it faces, and two directions along it (U and V) chosen so that
    // cross(U, V) = Normal. Then the corners -U-V, +U-V, +U+V, -U+V go counter-clockwise when
    // seen from outside.
    struct Face {
        glm::vec3 Normal;
        glm::vec3 U;
        glm::vec3 V;
    };
    constexpr Face kFaces[] = {
        { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },  // +X
        { { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } }, // -X
        { { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },  // +Y
        { { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // -Y
        { { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },  // +Z
        { { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }, // -Z
    };
    constexpr glm::vec2 kCorners[] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };

    MeshData mesh;
    for (const Face& face : kFaces) {
        std::array<Vertex, 4> corners;
        for (size_t i = 0; i < corners.size(); ++i) {
            const glm::vec2 corner = kCorners[i];
            // The corner from -1..1 along U and V becomes UV 0..1, with V flipped: the texture's
            // top row (v = 0) goes at the +V edge.
            corners[i] = {
                .Position = 0.5f * (face.Normal + corner.x * face.U + corner.y * face.V),
                .Color = kWhite,
                .UV = { (corner.x + 1.0f) / 2.0f, (1.0f - corner.y) / 2.0f },
            };
        }
        AddQuad(mesh, corners);
    }
    return mesh;
}

MeshData Plane(float width, float length, const glm::vec2& textureRepeats)
{
    const float halfWidth = width / 2.0f;
    const float halfLength = length / 2.0f;
    // Seen from above (-Z at the top), these corners go top-left, bottom-left, bottom-right,
    // top-right: counter-clockwise, so the plane's front faces up.
    MeshData mesh;
    AddQuad(mesh, { {
        { .Position = { -halfWidth, 0.0f, -halfLength }, .Color = kWhite, .UV = { 0.0f, 0.0f } },
        { .Position = { -halfWidth, 0.0f, halfLength }, .Color = kWhite, .UV = { 0.0f, textureRepeats.y } },
        { .Position = { halfWidth, 0.0f, halfLength }, .Color = kWhite, .UV = textureRepeats },
        { .Position = { halfWidth, 0.0f, -halfLength }, .Color = kWhite, .UV = { textureRepeats.x, 0.0f } },
    } });
    return mesh;
}

} // namespace Viva::Primitives
