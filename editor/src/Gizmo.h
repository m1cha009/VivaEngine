#pragma once

#include "SceneViewport.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <imgui.h>

#include <array>
#include <optional>
#include <vector>

namespace Viva {
class Transform;
}

// The three tools of Unity's Scene view, switched with W, E and R.
enum class GizmoTool {
    Move,
    Rotate,
    Scale,
};

// What the mouse did over the Scene view's image this frame.
struct GizmoInput {
    glm::vec2 Mouse { 0.0f };
    bool Pressed = false; // the left button went down over the image this frame
    bool Held = false;    // the left button is down, since a press over the image
    bool Snap = false;    // Ctrl is held: moves go in whole units, turns in 15 degrees
};

// The handles drawn on the selected object, which drag it around: arrows to move it along an axis
// (and small squares to move it along two), rings to turn it, and boxes to scale it, like Unity's
// Move, Rotate and Scale tools. Hand-written rather than a library (ImGuizmo), to see how it's done:
// it's all drawn with ImGui's draw list over the image, like the selection outline, and the mouse
// is tested against the handles as they appear on screen.
class Gizmo {
public:
    GizmoTool Tool = GizmoTool::Move;
    // The handles follow the object's own axes (Unity's "Local") or the world's ("Global"). Scale
    // always uses the object's own axes, as in Unity: it can only stretch along those.
    bool Local = true;

    // Draws the handles on `target` and drags it while a handle is held. Returns true if it moved,
    // turned or scaled the target this frame.
    bool Update(const SceneViewport& viewport, ImDrawList& drawList, Viva::Transform& target, const GizmoInput& input);

    // A handle was pressed and is being dragged: that press doesn't select anything.
    bool IsDragging() const { return m_Drag.Grabbed.Kind != HandleKind::None; }

private:
    enum class HandleKind {
        None,
        Axis,   // Move and Scale: along one axis
        Plane,  // Move: along the two axes other than Axis
        Ring,   // Rotate: around one axis
        Center, // Scale: all three axes at once
    };
    struct Handle {
        HandleKind Kind = HandleKind::None;
        int Axis = 0; // 0 = X, 1 = Y, 2 = Z
        bool operator==(const Handle&) const = default;
    };

    // Where the gizmo is this frame: its center (in the world and on screen), its three axes
    // (length 1), and how long its handles are in the world, which keeps them the same size on
    // screen.
    struct Frame {
        glm::vec3 Origin { 0.0f };
        glm::vec2 Center { 0.0f };
        glm::vec3 Axes[3];
        float Size = 1.0f;
    };

    // The handles as they appear on screen this frame: worked out once, then both tested against
    // the mouse and drawn, so what's grabbed is exactly what's seen.
    struct RingSegment {
        glm::vec2 From { 0.0f };
        glm::vec2 To { 0.0f };
        float Angle = 0.0f; // where on the ring it starts, in radians
        bool Front = true;  // on the half facing the camera
    };
    struct ScreenHandles {
        std::optional<glm::vec2> AxisTips[3];                // null: the axis points at the camera
        std::optional<std::array<glm::vec2, 4>> Planes[3];   // Move's squares
        std::vector<RingSegment> Rings[3];                   // Rotate's rings
    };

    // Null when the target is behind the camera: then there's nothing to draw or grab.
    std::optional<Frame> MakeFrame(const SceneViewport& viewport, const Viva::Transform& target) const;
    ScreenHandles Project(const SceneViewport& viewport, const Frame& frame) const;
    Handle HitTest(const Frame& frame, const ScreenHandles& screen, const glm::vec2& mouse) const;
    void Draw(ImDrawList& drawList, const Frame& frame, const ScreenHandles& screen, Handle highlighted) const;
    void BeginDrag(const SceneViewport& viewport, const Frame& frame, const ScreenHandles& screen,
                   const Viva::Transform& target, Handle handle, const glm::vec2& mouse);
    bool Drag(const SceneViewport& viewport, Viva::Transform& target, const GizmoInput& input);

    // The drag in progress: the handle, and the target and mouse as they were when it started.
    // Each frame works out the change from the start, rather than adding up small steps, so
    // snapping and rounding errors don't build up.
    struct DragState {
        Handle Grabbed; // None while nothing is dragged
        Frame StartFrame;
        glm::vec2 StartMouse { 0.0f };
        bool Moved = false; // the mouse has moved since the press: until then, nothing changes
        glm::quat StartRotation = glm::identity<glm::quat>(); // in the world
        glm::vec3 StartScale { 1.0f };    // local
        float StartAlongAxis = 0.0f;      // Move along an axis: where the mouse ray met it
        glm::vec3 StartOnPlane { 0.0f };  // Move along a plane: where the mouse ray met it
        glm::vec2 RingTangent { 0.0f };   // Rotate: the screen direction a positive turn moves the grabbed point
    };
    DragState m_Drag;
};
