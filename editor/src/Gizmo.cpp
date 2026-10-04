#include "Gizmo.h"

#include "Viva/Transform.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

using namespace Viva;

namespace {

// Sizes on screen, in pixels (points).
constexpr float kHandleLength = 100.0f; // an arrow, a scale handle, a ring's radius
constexpr float kPickDistance = 8.0f;   // how close the mouse must be to a line to grab it
constexpr float kMinAxisPixels = 10.0f; // an axis shorter than this on screen points at the camera
constexpr float kArrowSize = 10.0f;
constexpr float kBoxSize = 8.0f;        // the scale handles' tips, and the center
constexpr float kPlaneNear = 0.2f;      // the plane squares, as fractions of the handle length
constexpr float kPlaneFar = 0.45f;
constexpr int kRingSegments = 64;
// A ring's segment is "behind" (drawn faintly, not grabbed) when the direction from the center to
// it points away from the camera by more than this cosine (about 17 degrees past side-on). A ring
// that faces the camera, seen a little from above, would otherwise lose its lower half.
constexpr float kBehindCosine = -0.3f;

// Snapping with Ctrl: Unity's defaults are a grid step and 15 degrees.
constexpr float kMoveSnap = 1.0f;
constexpr float kRotateSnap = glm::radians(15.0f);

// The axes' colors, as everywhere in Unity (and on the grid): X red, Y green, Z blue. A handle
// under the mouse, or being dragged, turns yellow.
constexpr ImU32 kAxisColors[3] = { IM_COL32(230, 60, 60, 255), IM_COL32(110, 200, 60, 255), IM_COL32(60, 110, 235, 255) };
constexpr ImU32 kHighlightColor = IM_COL32(255, 220, 60, 255);
constexpr ImU32 kCenterColor = IM_COL32(220, 220, 220, 255);

ImU32 WithAlpha(ImU32 color, int alpha)
{
    return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

float Cross(const glm::vec2& a, const glm::vec2& b)
{
    // The 2D cross product: positive when b is turned clockwise from a on screen (y runs down).
    return a.x * b.y - a.y * b.x;
}

// How far `point` is from the segment from a to b, on screen.
float DistanceToSegment(const glm::vec2& point, const glm::vec2& a, const glm::vec2& b)
{
    const glm::vec2 ab = b - a;
    const float lengthSquared = glm::dot(ab, ab);
    const float t = lengthSquared > 0.0f ? std::clamp(glm::dot(point - a, ab) / lengthSquared, 0.0f, 1.0f) : 0.0f;
    return glm::length(point - (a + ab * t));
}

// Whether `point` is inside the four-cornered shape (corners in order around it): on the same
// side of all four edges.
bool InsideQuad(const glm::vec2& point, const std::array<glm::vec2, 4>& corners)
{
    bool positive = false;
    bool negative = false;
    for (int i = 0; i < 4; ++i) {
        const float side = Cross(corners[(i + 1) % 4] - corners[i], point - corners[i]);
        positive |= side > 0.0f;
        negative |= side < 0.0f;
    }
    return !(positive && negative);
}

// The other two axes than `axis`, in order: for X, Y and Z; for Y, Z and X; for Z, X and Y.
int Next(int axis) { return (axis + 1) % 3; }
int After(int axis) { return (axis + 2) % 3; }

// Where on the line through `point` along `axis` (length 1) the ray passes closest: the distance
// along the axis. std::nullopt when they run (almost) parallel and every point is as close. It's
// the closest-points formula for two lines, with both directions of length 1.
std::optional<float> ClosestAlongAxis(const Ray& ray, const glm::vec3& point, const glm::vec3& axis)
{
    const glm::vec3 between = point - ray.Origin;
    const float b = glm::dot(axis, ray.Direction);
    const float d = glm::dot(axis, between);
    const float e = glm::dot(ray.Direction, between);
    const float denominator = 1.0f - b * b;
    if (denominator < 1e-4f)
        return std::nullopt;
    return (b * e - d) / denominator;
}

// Where the ray meets the plane through `point` facing `normal`, or std::nullopt if it runs along
// the plane or away from it.
std::optional<glm::vec3> IntersectPlane(const Ray& ray, const glm::vec3& point, const glm::vec3& normal)
{
    const float facing = glm::dot(ray.Direction, normal);
    if (std::abs(facing) < 1e-4f)
        return std::nullopt;
    const float distance = glm::dot(point - ray.Origin, normal) / facing;
    if (distance < 0.0f)
        return std::nullopt;
    return ray.Origin + ray.Direction * distance;
}

float Snapped(float value, float step)
{
    return std::round(value / step) * step;
}

} // namespace

std::optional<Gizmo::Frame> Gizmo::MakeFrame(const SceneViewport& viewport, const Transform& target) const
{
    Frame frame;
    frame.Origin = target.GetPosition();
    const std::optional<glm::vec2> center = viewport.ToScreen(frame.Origin);
    if (!center)
        return std::nullopt;
    frame.Center = *center;
    // The object's axes in the world are its rotation applied to X, Y and Z. (Turning a vector
    // keeps its length, so they stay length 1.)
    const glm::quat rotation = Local || Tool == GizmoTool::Scale ? target.GetRotation() : glm::identity<glm::quat>();
    for (int i = 0; i < 3; ++i) {
        glm::vec3 axis(0.0f);
        axis[i] = 1.0f;
        frame.Axes[i] = rotation * axis;
    }
    frame.Size = viewport.PixelsToWorld(kHandleLength, frame.Origin);
    return frame;
}

bool Gizmo::Update(const SceneViewport& viewport, ImDrawList& drawList, Transform& target, const GizmoInput& input)
{
    bool changed = false;
    if (IsDragging()) {
        if (input.Held)
            changed = Drag(viewport, target, input);
        else
            m_Drag = {}; // let go: the drag is over
    }

    const std::optional<Frame> frame = MakeFrame(viewport, target);
    if (!frame) {
        m_Drag = {};
        return changed;
    }
    const ScreenHandles screen = Project(viewport, *frame);
    Handle highlighted = m_Drag.Grabbed;
    if (!IsDragging()) {
        highlighted = HitTest(*frame, screen, input.Mouse);
        if (input.Pressed && highlighted.Kind != HandleKind::None)
            BeginDrag(viewport, *frame, screen, target, highlighted, input.Mouse);
    }
    Draw(drawList, *frame, screen, highlighted);
    return changed;
}

Gizmo::ScreenHandles Gizmo::Project(const SceneViewport& viewport, const Frame& frame) const
{
    ScreenHandles screen;
    for (int axis = 0; axis < 3; ++axis) {
        // In world space, the two axes across this one, scaled to the handle size.
        const glm::vec3 u = frame.Axes[Next(axis)] * frame.Size;
        const glm::vec3 v = frame.Axes[After(axis)] * frame.Size;

        if (Tool == GizmoTool::Rotate) {
            // The ring around this axis, as short straight segments.
            const glm::vec3 toCamera = glm::normalize(viewport.CameraPosition - frame.Origin);
            for (int i = 0; i < kRingSegments; ++i) {
                const float a0 = glm::two_pi<float>() * static_cast<float>(i) / kRingSegments;
                const float a1 = glm::two_pi<float>() * static_cast<float>(i + 1) / kRingSegments;
                const glm::vec3 p0 = u * std::cos(a0) + v * std::sin(a0);
                const glm::vec3 p1 = u * std::cos(a1) + v * std::sin(a1);
                const std::optional<glm::vec2> s0 = viewport.ToScreen(frame.Origin + p0);
                const std::optional<glm::vec2> s1 = viewport.ToScreen(frame.Origin + p1);
                if (!s0 || !s1)
                    continue;
                const bool front = glm::dot(glm::normalize(p0 + p1), toCamera) >= kBehindCosine;
                screen.Rings[axis].push_back({ *s0, *s1, a0, front });
            }
            continue;
        }

        const std::optional<glm::vec2> tip = viewport.ToScreen(frame.Origin + frame.Axes[axis] * frame.Size);
        if (tip && glm::length(*tip - frame.Center) >= kMinAxisPixels)
            screen.AxisTips[axis] = tip;

        if (Tool == GizmoTool::Move) {
            // The square in the plane across this axis, near the center.
            const glm::vec3 corners[4] = { u * kPlaneNear + v * kPlaneNear, u * kPlaneFar + v * kPlaneNear,
                                           u * kPlaneFar + v * kPlaneFar, u * kPlaneNear + v * kPlaneFar };
            std::array<glm::vec2, 4> onScreen;
            bool visible = true;
            for (int i = 0; i < 4; ++i) {
                const std::optional<glm::vec2> corner = viewport.ToScreen(frame.Origin + corners[i]);
                visible &= corner.has_value();
                onScreen[i] = corner.value_or(glm::vec2(0.0f));
            }
            if (visible)
                screen.Planes[axis] = onScreen;
        }
    }
    return screen;
}

Gizmo::Handle Gizmo::HitTest(const Frame& frame, const ScreenHandles& screen, const glm::vec2& mouse) const
{
    // Scale's center box comes first: it sits where all three axes start.
    if (Tool == GizmoTool::Scale && glm::length(mouse - frame.Center) < kBoxSize)
        return { HandleKind::Center, 0 };

    // Then Move's plane squares.
    for (int axis = 0; axis < 3; ++axis) {
        if (screen.Planes[axis] && InsideQuad(mouse, *screen.Planes[axis]))
            return { HandleKind::Plane, axis };
    }

    // Then the nearest line within reach: an axis, or a ring's front half (the back half is drawn
    // faintly, behind the object, and can't be grabbed).
    Handle nearest;
    float nearestDistance = kPickDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (screen.AxisTips[axis]) {
            const float distance = DistanceToSegment(mouse, frame.Center, *screen.AxisTips[axis]);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = { HandleKind::Axis, axis };
            }
        }
        for (const RingSegment& segment : screen.Rings[axis]) {
            const float distance = DistanceToSegment(mouse, segment.From, segment.To);
            if (segment.Front && distance < nearestDistance) {
                nearestDistance = distance;
                nearest = { HandleKind::Ring, axis };
            }
        }
    }
    return nearest;
}

void Gizmo::Draw(ImDrawList& drawList, const Frame& frame, const ScreenHandles& screen, Handle highlighted) const
{
    const auto colorOf = [&](Handle handle) {
        return handle == highlighted ? kHighlightColor : kAxisColors[handle.Axis];
    };
    for (int axis = 0; axis < 3; ++axis) {
        // Rotate's rings: the half facing the camera solid, the back half faint.
        const ImU32 ringColor = colorOf({ HandleKind::Ring, axis });
        for (const RingSegment& segment : screen.Rings[axis]) {
            drawList.AddLine(ToImVec2(segment.From), ToImVec2(segment.To),
                             segment.Front ? ringColor : WithAlpha(ringColor, 60), segment.Front ? 2.5f : 1.5f);
        }

        // Move's plane squares, each colored like the axis it doesn't move along (as in Unity).
        if (const std::optional<std::array<glm::vec2, 4>>& plane = screen.Planes[axis]) {
            const ImU32 color = colorOf({ HandleKind::Plane, axis });
            const ImVec2 corners[4] = { ToImVec2((*plane)[0]), ToImVec2((*plane)[1]), ToImVec2((*plane)[2]),
                                        ToImVec2((*plane)[3]) };
            drawList.AddConvexPolyFilled(corners, 4, WithAlpha(color, 90));
            drawList.AddQuad(corners[0], corners[1], corners[2], corners[3], color, 1.5f);
        }

        // The axes: a line from the center, with an arrowhead (Move) or a box (Scale) at its tip.
        if (const std::optional<glm::vec2>& tip = screen.AxisTips[axis]) {
            const ImU32 color = colorOf({ HandleKind::Axis, axis });
            drawList.AddLine(ToImVec2(frame.Center), ToImVec2(*tip), color, 2.5f);
            const glm::vec2 direction = glm::normalize(*tip - frame.Center);
            if (Tool == GizmoTool::Move) {
                // A triangle pointing on along the axis: its tip, and two corners beside the line.
                const glm::vec2 side(-direction.y, direction.x);
                drawList.AddTriangleFilled(ToImVec2(*tip + direction * kArrowSize * 1.5f),
                                           ToImVec2(*tip + side * kArrowSize * 0.6f),
                                           ToImVec2(*tip - side * kArrowSize * 0.6f), color);
            } else {
                const glm::vec2 half(kBoxSize * 0.6f);
                drawList.AddRectFilled(ToImVec2(*tip - half), ToImVec2(*tip + half), color);
            }
        }
    }
    if (Tool == GizmoTool::Scale) {
        const ImU32 color = highlighted.Kind == HandleKind::Center ? kHighlightColor : kCenterColor;
        const glm::vec2 half(kBoxSize * 0.7f);
        drawList.AddRectFilled(ToImVec2(frame.Center - half), ToImVec2(frame.Center + half), color);
    }
}

void Gizmo::BeginDrag(const SceneViewport& viewport, const Frame& frame, const ScreenHandles& screen,
                      const Transform& target, Handle handle, const glm::vec2& mouse)
{
    m_Drag = {};
    m_Drag.Grabbed = handle;
    m_Drag.StartFrame = frame;
    m_Drag.StartMouse = mouse;
    m_Drag.StartRotation = target.GetRotation();
    m_Drag.StartScale = target.LocalScale;

    const Ray ray = viewport.RayThrough(mouse);
    if (handle.Kind == HandleKind::Axis)
        m_Drag.StartAlongAxis = ClosestAlongAxis(ray, frame.Origin, frame.Axes[handle.Axis]).value_or(0.0f);
    if (handle.Kind == HandleKind::Plane)
        m_Drag.StartOnPlane = IntersectPlane(ray, frame.Origin, frame.Axes[handle.Axis]).value_or(frame.Origin);
    if (handle.Kind == HandleKind::Ring) {
        // The point of the ring that was grabbed (the front segment nearest the mouse), and the
        // way it moves when the object turns a little the positive way: around axis A, a point
        // at r from the center moves along A x r (the cross product; the right-hand rule). Seen
        // on screen, that's the direction to drag in. It works the same from any side, even with
        // the ring seen nearly edge-on.
        const RingSegment* grabbed = nullptr;
        float nearest = 0.0f;
        for (const RingSegment& segment : screen.Rings[handle.Axis]) {
            const float distance = DistanceToSegment(mouse, segment.From, segment.To);
            if (segment.Front && (!grabbed || distance < nearest)) {
                grabbed = &segment;
                nearest = distance;
            }
        }
        if (grabbed) {
            const glm::vec3 axis = frame.Axes[handle.Axis];
            const glm::vec3 radius = (frame.Axes[Next(handle.Axis)] * std::cos(grabbed->Angle) +
                                      frame.Axes[After(handle.Axis)] * std::sin(grabbed->Angle)) * frame.Size;
            const glm::vec3 along = glm::cross(axis, radius) * 0.1f; // a little way along the tangent
            const std::optional<glm::vec2> from = viewport.ToScreen(frame.Origin + radius);
            const std::optional<glm::vec2> to = viewport.ToScreen(frame.Origin + radius + along);
            if (from && to && glm::length(*to - *from) > 0.0f)
                m_Drag.RingTangent = glm::normalize(*to - *from);
        }
    }
}

bool Gizmo::Drag(const SceneViewport& viewport, Transform& target, const GizmoInput& input)
{
    // A press that hasn't moved yet changes nothing, so a click on a handle makes no undo step.
    if (!m_Drag.Moved) {
        if (glm::length(input.Mouse - m_Drag.StartMouse) < 1.0f)
            return false;
        m_Drag.Moved = true;
    }

    const Frame& frame = m_Drag.StartFrame; // the axes stay as they were when the drag began
    const int axis = m_Drag.Grabbed.Axis;
    const Ray ray = viewport.RayThrough(input.Mouse);

    switch (m_Drag.Grabbed.Kind) {
    case HandleKind::Axis:
        if (Tool == GizmoTool::Move) {
            // The object follows the point of the axis closest to the mouse ray: the distance
            // along the axis from where the drag began is how far it moves.
            const std::optional<float> along = ClosestAlongAxis(ray, frame.Origin, frame.Axes[axis]);
            if (!along)
                return false;
            float moved = *along - m_Drag.StartAlongAxis;
            if (input.Snap)
                moved = Snapped(moved, kMoveSnap);
            target.SetPosition(frame.Origin + frame.Axes[axis] * moved);
        } else {
            // Scale along one axis: how far the mouse moved along the axis's line on screen,
            // compared with the handle's length there. Dragging out to twice the length doubles it.
            const std::optional<glm::vec2> tip = viewport.ToScreen(frame.Origin + frame.Axes[axis] * frame.Size);
            if (!tip || glm::length(*tip - frame.Center) < 1.0f)
                return false;
            const glm::vec2 onScreen = *tip - frame.Center;
            const float moved = glm::dot(input.Mouse - m_Drag.StartMouse, glm::normalize(onScreen));
            glm::vec3 scale = m_Drag.StartScale;
            scale[axis] *= std::max(1.0f + moved / glm::length(onScreen), 0.01f);
            target.LocalScale = scale;
        }
        return true;

    case HandleKind::Plane: {
        // The object follows the point of the plane under the mouse, in both of its directions.
        const std::optional<glm::vec3> onPlane = IntersectPlane(ray, frame.Origin, frame.Axes[axis]);
        if (!onPlane)
            return false;
        const glm::vec3 moved = *onPlane - m_Drag.StartOnPlane;
        float u = glm::dot(moved, frame.Axes[Next(axis)]);
        float v = glm::dot(moved, frame.Axes[After(axis)]);
        if (input.Snap) {
            u = Snapped(u, kMoveSnap);
            v = Snapped(v, kMoveSnap);
        }
        target.SetPosition(frame.Origin + frame.Axes[Next(axis)] * u + frame.Axes[After(axis)] * v);
        return true;
    }

    case HandleKind::Ring: {
        // Dragging along the grabbed point's tangent turns the object: a drag as long as the
        // ring's radius on screen is one radian, as if the mouse pulled the ring round.
        float angle = glm::dot(input.Mouse - m_Drag.StartMouse, m_Drag.RingTangent) / kHandleLength;
        if (input.Snap)
            angle = Snapped(angle, kRotateSnap);
        // Turning by `angle` around the axis, after the rotation the object had: a rotation
        // multiplied on the left is applied after the one on the right.
        target.SetRotation(glm::normalize(glm::angleAxis(angle, frame.Axes[axis]) * m_Drag.StartRotation));
        return true;
    }

    case HandleKind::Center: {
        // Scale all three axes alike: right or up makes it bigger, left or down smaller.
        const glm::vec2 moved = input.Mouse - m_Drag.StartMouse;
        const float factor = std::max(1.0f + (moved.x - moved.y) / kHandleLength, 0.01f);
        target.LocalScale = m_Drag.StartScale * factor;
        return true;
    }

    case HandleKind::None:
        break;
    }
    return false;
}
