#include "Viva/Primitives.h"

#include <glm/gtc/constants.hpp>

#include <array>
#include <cmath>

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

// How many slices the round primitives are cut into around Y, and the sphere from top to bottom.
// More look rounder and cost more vertices; these are about what Unity's have.
constexpr uint32_t kSegments = 32;
constexpr uint32_t kRings = 16;

// A point on a circle of radius 0.5 around the Y axis, `segment` slices of kSegments around from
// +X. The angle grows towards -Z: seen from outside, that's to the right, which is the way the
// texture's U runs, so the texture isn't mirrored.
glm::vec2 CirclePoint(uint32_t segment)
{
    const float angle = glm::two_pi<float>() * static_cast<float>(segment) / static_cast<float>(kSegments);
    return { 0.5f * std::cos(angle), -0.5f * std::sin(angle) }; // x, z
}

// Adds the triangles of a grid of vertices, `columns` + 1 per row, starting at `first`. Each cell
// of the grid is the square between rows r and r + 1, columns c and c + 1, made of two triangles.
// Row 0 must be at the top and columns must grow to the right, seen from the front.
void AddGrid(MeshData& mesh, uint32_t first, uint32_t rows, uint32_t columns)
{
    for (uint32_t row = 0; row < rows; ++row) {
        for (uint32_t column = 0; column < columns; ++column) {
            const uint32_t topLeft = first + row * (columns + 1) + column;
            const uint32_t bottomLeft = topLeft + columns + 1;
            // Top-left, bottom-left, bottom-right, top-right: counter-clockwise, as AddQuad wants.
            mesh.Indices.insert(mesh.Indices.end(), { topLeft, bottomLeft, bottomLeft + 1, topLeft, bottomLeft + 1, topLeft + 1 });
        }
    }
}

// A flat disc at height y closing one end of the cylinder, facing up (+Y) or down: a vertex in the
// middle and a fan of triangles around it. The texture lies across it as seen from outside.
void AddCap(MeshData& mesh, float y, bool facingUp)
{
    const auto center = static_cast<uint32_t>(mesh.Vertices.size());
    mesh.Vertices.push_back({ .Position = { 0.0f, y, 0.0f }, .Color = kWhite, .UV = { 0.5f, 0.5f } });
    for (uint32_t segment = 0; segment <= kSegments; ++segment) {
        const glm::vec2 point = CirclePoint(segment);
        // From above, -Z is up on the screen (as for Plane), so a point's z gives its V directly.
        // From below, the picture is mirrored left to right, so U flips.
        const float u = facingUp ? 0.5f + point.x : 0.5f - point.x;
        mesh.Vertices.push_back({ .Position = { point.x, y, point.y }, .Color = kWhite, .UV = { u, 0.5f + point.y } });
    }
    // Seen from above, the points go counter-clockwise as the angle grows (towards -Z), so center,
    // point, next point is a front face from above. From below it's the other way round.
    for (uint32_t segment = 0; segment < kSegments; ++segment) {
        const uint32_t point = center + 1 + segment;
        if (facingUp)
            mesh.Indices.insert(mesh.Indices.end(), { center, point, point + 1 });
        else
            mesh.Indices.insert(mesh.Indices.end(), { center, point + 1, point });
    }
}

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

MeshData Plane()
{
    constexpr float kHalf = kPlaneSize / 2.0f;
    // Seen from above (-Z at the top), these corners go top-left, bottom-left, bottom-right,
    // top-right: counter-clockwise, so the plane's front faces up.
    MeshData mesh;
    AddQuad(mesh, { {
        { .Position = { -kHalf, 0.0f, -kHalf }, .Color = kWhite, .UV = { 0.0f, 0.0f } },
        { .Position = { -kHalf, 0.0f, kHalf }, .Color = kWhite, .UV = { 0.0f, 1.0f } },
        { .Position = { kHalf, 0.0f, kHalf }, .Color = kWhite, .UV = { 1.0f, 1.0f } },
        { .Position = { kHalf, 0.0f, -kHalf }, .Color = kWhite, .UV = { 1.0f, 0.0f } },
    } });
    return mesh;
}

MeshData Sphere()
{
    // A grid wrapped around the ball: kRings + 1 rows from the top pole to the bottom one, each a
    // circle of kSegments + 1 vertices. The last vertex of a row sits where its first does, but
    // with U = 1 instead of 0: that's the seam where the texture's right edge meets its left. The
    // whole top row sits at the pole, so the first ring of triangles has one corner twice, which
    // makes half of its triangles empty: harmless, and it keeps the grid simple.
    MeshData mesh;
    for (uint32_t ring = 0; ring <= kRings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(kRings);
        const float polarAngle = glm::pi<float>() * v; // 0 at the top, pi at the bottom
        const float y = 0.5f * std::cos(polarAngle);
        const float ringScale = std::sin(polarAngle); // 0 at the poles, 1 around the middle
        for (uint32_t segment = 0; segment <= kSegments; ++segment) {
            const glm::vec2 point = CirclePoint(segment) * ringScale;
            const float u = static_cast<float>(segment) / static_cast<float>(kSegments);
            mesh.Vertices.push_back({ .Position = { point.x, y, point.y }, .Color = kWhite, .UV = { u, v } });
        }
    }
    AddGrid(mesh, 0, kRings, kSegments);
    return mesh;
}

MeshData Cylinder()
{
    // The side: a grid of two rows, the top circle and the bottom one.
    MeshData mesh;
    for (const float y : { 1.0f, -1.0f }) {
        for (uint32_t segment = 0; segment <= kSegments; ++segment) {
            const glm::vec2 point = CirclePoint(segment);
            const float u = static_cast<float>(segment) / static_cast<float>(kSegments);
            const float v = y > 0.0f ? 0.0f : 1.0f;
            mesh.Vertices.push_back({ .Position = { point.x, y, point.y }, .Color = kWhite, .UV = { u, v } });
        }
    }
    AddGrid(mesh, 0, 1, kSegments);

    // The ends have vertices of their own: the same positions as the side's edges, but other
    // texture coordinates.
    AddCap(mesh, 1.0f, true);
    AddCap(mesh, -1.0f, false);
    return mesh;
}

} // namespace Viva::Primitives
