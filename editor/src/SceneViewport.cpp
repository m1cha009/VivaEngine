#include "SceneViewport.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

namespace {

// A point in clip space (after the projection, before dividing by w) onto the image: divide by w
// (the perspective divide) for "normalized device coordinates", -1 to 1 across the image, then
// map those to its corners. The projection is OpenGL-style, y up, while screen y runs down.
glm::vec2 ClipToScreen(const SceneViewport& viewport, const glm::vec4& clip)
{
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    return viewport.ImageMin + glm::vec2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f) * viewport.ImageSize;
}

} // namespace

std::optional<glm::vec2> SceneViewport::ToScreen(const glm::vec3& world) const
{
    // In clip space the near plane is z = 0, and everything in front of the camera has z >= 0.
    const glm::vec4 clip = Projection * (View * glm::vec4(world, 1.0f));
    if (clip.z < 0.0f)
        return std::nullopt;
    return ClipToScreen(*this, clip);
}

Ray SceneViewport::RayThrough(const glm::vec2& point) const
{
    // The point in normalized device coordinates (see ClipToScreen, backwards).
    const glm::vec2 uv = (point - ImageMin) / ImageSize;
    const glm::vec2 ndc(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    // The projection squeezed everything the camera sees into a box where depth runs from 0 (the
    // near plane) to 1 (the far plane). Its inverse takes that box back into the world: the point
    // at depth 0 lies on the near plane, the one at depth 1 on the far plane, and the ray runs
    // from one to the other. The division by w undoes the perspective divide.
    const glm::mat4 toWorld = glm::inverse(Projection * View);
    glm::vec4 nearPoint = toWorld * glm::vec4(ndc, 0.0f, 1.0f);
    glm::vec4 farPoint = toWorld * glm::vec4(ndc, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    return { .Origin = glm::vec3(nearPoint), .Direction = glm::normalize(glm::vec3(farPoint - nearPoint)) };
}

float SceneViewport::PixelsToWorld(float pixels, const glm::vec3& at) const
{
    // After the projection, w is the point's distance in front of the camera, and Projection[1][1]
    // is 1 / tan(half the field of view). So the image's height covers 2 * w / Projection[1][1]
    // world units at that distance.
    const float depth = (Projection * (View * glm::vec4(at, 1.0f))).w;
    return pixels * 2.0f * depth / (Projection[1][1] * ImageSize.y);
}

void SceneViewport::DrawLine(ImDrawList& drawList, const glm::vec3& from, const glm::vec3& to, ImU32 color,
                             float thickness) const
{
    // Into clip space. A line reaching behind the camera is cut where it crosses the near plane
    // first: projecting a point behind the camera would flip it to the wrong side of the picture.
    glm::vec4 a = Projection * (View * glm::vec4(from, 1.0f));
    glm::vec4 b = Projection * (View * glm::vec4(to, 1.0f));
    if (a.z < 0.0f && b.z < 0.0f)
        return; // all behind
    if (a.z < 0.0f)
        a = glm::mix(a, b, a.z / (a.z - b.z));
    else if (b.z < 0.0f)
        b = glm::mix(b, a, b.z / (b.z - a.z));
    drawList.AddLine(ToImVec2(ClipToScreen(*this, a)), ToImVec2(ClipToScreen(*this, b)), color, thickness);
}
