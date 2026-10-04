#pragma once

#include "Viva/Component.h"
#include "Viva/FieldVisitor.h"
#include "Viva/Renderer.h"

#include <memory>
#include <utility>
#include <vector>

namespace Viva {

// One piece of what a MeshRenderer draws: a mesh, and the material it's drawn with.
struct MeshPart {
    std::shared_ptr<Viva::Mesh> Mesh;
    std::shared_ptr<Viva::Material> Material;
};

// Makes a GameObject visible: every frame, the scene draws each of its parts where the
// GameObject's Transform puts it. Unity's MeshFilter and MeshRenderer in one component.
//
// Most objects have one part. A model's mesh can have several, each with its own material, like
// the milk truck's body, glass and window trim. Unity keeps those in one Mesh as submeshes and
// gives the MeshRenderer one material per submesh (its materials array). Here each part simply
// has a mesh of its own.
class MeshRenderer : public Component {
public:
    // Nothing to draw yet: for scene files, which fill in the parts (and for adding parts later).
    MeshRenderer() = default;

    // One mesh, drawn with one material.
    MeshRenderer(std::shared_ptr<Viva::Mesh> mesh, std::shared_ptr<Viva::Material> material)
    {
        Parts.push_back({ std::move(mesh), std::move(material) });
    }

    explicit MeshRenderer(std::vector<MeshPart> parts)
        : Parts(std::move(parts))
    {
    }

    // Can be changed at any time. A part without both a mesh and a material isn't drawn.
    std::vector<MeshPart> Parts;

    void VisitFields(FieldVisitor& fields) override
    {
        fields.List("Parts", Parts, [](FieldVisitor& part, MeshPart& element) {
            part.Field("Mesh", element.Mesh);
            part.Field("Material", element.Material);
        });
    }
};

} // namespace Viva
