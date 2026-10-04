#version 450

// The editor's ground grid (Renderer::DrawGrid): one big square on the plane y = 0, centered
// under the camera, so wherever the camera goes the grid reaches the horizon. The lines
// themselves are drawn by the fragment shader, pixel by pixel.

// The same camera as every other draw (see Unlit.vert).
layout(set = 0, binding = 0) uniform Camera {
    mat4 View;
    mat4 Projection;
} camera;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outCameraPosition;

// How far the square reaches from the camera: past where the lines fade out (Grid.frag).
const float kHalfSize = 1000.0;

// No vertex buffer: the six corners of the square's two triangles are written here, and
// gl_VertexIndex (0 to 5, from vkCmdDraw) picks one. The pipeline doesn't cull, so their order
// doesn't matter.
const vec2 kCorners[6] = vec2[](
    vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0),
    vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(1.0, -1.0));

void main()
{
    // The view matrix moves the camera to the origin, so its inverse moves the origin to the
    // camera: its last column is where the camera is.
    const vec3 cameraPosition = inverse(camera.View)[3].xyz;
    const vec2 corner = cameraPosition.xz + kCorners[gl_VertexIndex] * kHalfSize;
    const vec4 world = vec4(corner.x, 0.0, corner.y, 1.0);
    outWorldPosition = world.xyz;
    outCameraPosition = cameraPosition;
    gl_Position = camera.Projection * (camera.View * world);
}
