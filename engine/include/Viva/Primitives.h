#pragma once

#include "Viva/MeshData.h"

// Built-in meshes, like the primitives Unity creates with GameObject.CreatePrimitive. Their
// vertices are white, so a material's texture and color show as they are; edit the returned
// MeshData to change that before creating a mesh from it.
namespace Viva::Primitives {

// A cube from -0.5 to 0.5 on every axis, with the whole texture on each face. The faces come in
// the order +X, -X, +Y, -Y, +Z, -Z, four vertices each (24 vertices, 36 indices): a corner is
// stored once per face because each face gives it different texture coordinates.
MeshData Cube();

// The built-in plane's size: 10 x 10 units, like Unity's.
inline constexpr float kPlaneSize = 10.0f;

// A flat square on the ground (y = 0), kPlaneSize units along X and Z, centered on the origin and
// facing up, with the whole texture across it once. Scale its Transform for other sizes, and set
// the material's Tiling to repeat the texture.
MeshData Plane();

// A ball 1 unit across (radius 0.5) around the origin, like Unity's sphere: rings of vertices from
// the top (+Y) to the bottom, each going around the Y axis. The texture wraps around it once, its
// top row at the top of the sphere, as a world map wraps a globe.
MeshData Sphere();

// A cylinder 1 unit across and 2 tall (y from -1 to 1) around the Y axis, like Unity's: its side,
// with the texture wrapped around once, and a flat disc closing each end, with the texture laid
// across it.
MeshData Cylinder();

} // namespace Viva::Primitives
