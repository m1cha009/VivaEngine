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

// A flat square on the ground (y = 0), size x size units, centered on the origin and facing up.
// The texture repeats `textureRepeats` times along each side.
MeshData Plane(float size, float textureRepeats = 1.0f);

} // namespace Viva::Primitives
