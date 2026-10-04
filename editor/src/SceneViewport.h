#pragma once

#include "Picking.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <imgui.h>

#include <optional>

// Where the Scene view's image is on screen this frame, and the camera it shows: everything needed
// to go between the world and the image. Points on the image are in ImGui's screen coordinates.
// The selection outline and the gizmos draw through it; clicks become rays through it.
struct SceneViewport {
    glm::vec2 ImageMin { 0.0f };  // the image's top-left corner
    glm::vec2 ImageSize { 0.0f };
    glm::mat4 View { 1.0f };
    glm::mat4 Projection { 1.0f }; // OpenGL-style, y up (see Viva/Camera.h)
    glm::vec3 CameraPosition { 0.0f };

    // Where a point in the world shows on the image, or std::nullopt if it's behind the camera.
    std::optional<glm::vec2> ToScreen(const glm::vec3& world) const;
    // The ray from the camera through a point of the image.
    Ray RayThrough(const glm::vec2& point) const;
    // How long a line `pixels` long on screen is in the world, at the distance of `at`: what keeps
    // a gizmo the same size on screen however far away its object is.
    float PixelsToWorld(float pixels, const glm::vec3& at) const;

    // A line between two points in the world, drawn over the image, cut where it goes behind the
    // camera.
    void DrawLine(ImDrawList& drawList, const glm::vec3& from, const glm::vec3& to, ImU32 color,
                  float thickness = 1.5f) const;
};

// ImGui's 2D vector from GLM's, for the draw list.
inline ImVec2 ToImVec2(const glm::vec2& v)
{
    return { v.x, v.y };
}
