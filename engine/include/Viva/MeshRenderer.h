#pragma once

#include "Viva/Component.h"
#include "Viva/Renderer.h"

#include <memory>
#include <utility>

namespace Viva {

// Makes a GameObject visible: every frame, the scene draws Mesh with Material where the
// GameObject's Transform puts it. Unity's MeshFilter and MeshRenderer in one component.
class MeshRenderer : public Component {
public:
    MeshRenderer(std::shared_ptr<Viva::Mesh> mesh, std::shared_ptr<Viva::Material> material)
        : Mesh(std::move(mesh))
        , Material(std::move(material))
    {
    }

    // Either can be swapped at any time. Without both, nothing is drawn.
    std::shared_ptr<Viva::Mesh> Mesh;
    std::shared_ptr<Viva::Material> Material;
};

} // namespace Viva
