#pragma once

#include "Viva/MeshRenderer.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Viva {

class GameObject;
class Renderer;
class Scene;

// A 3D model loaded from a glTF file: its meshes, materials and textures, uploaded to the GPU
// once, and the hierarchy of nodes that places them. Loading puts nothing in the scene:
// Instantiate creates GameObjects from the model, as many times as needed. That's how a model
// asset works in Unity too: it's a prefab, and Instantiate turns it into objects in the scene.
// Every instance shares the model's meshes, materials and textures, so a second truck costs no
// extra GPU memory.
//
// glTF ("GL Transmission Format") is the Khronos Group's file format for 3D models, which most 3D
// tools export. A .gltf file is JSON describing the model, next to .bin files with the vertex data
// and the image files; a .glb file packs all of it into one binary file.
class Model {
public:
    // Loads a .gltf or .glb file from the assets folder, for example "models/CesiumMilkTruck.glb".
    // Returns nullptr (after logging why) if it can't. Like creating meshes and textures, this
    // waits for the uploads to the GPU to finish: load models while loading (OnStart).
    static std::unique_ptr<Model> Load(Renderer& renderer, const std::string& assetName);

    Model() = default; // creates nothing: use Load()

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Creates the model's GameObjects below `parent`, or at the top of the hierarchy: one named
    // after the model, with the file's nodes below it. Each node becomes a GameObject with the
    // node's name and local position, rotation and scale, and with a MeshRenderer if the node has
    // a mesh. Returns the top object, which places the whole model. Unity's Instantiate(prefab).
    GameObject& Instantiate(Scene& scene, GameObject* parent = nullptr) const;

private:
    // A node: one point in the file's hierarchy, placed relative to its parent, which may show a
    // mesh.
    struct Node {
        std::string Name;
        glm::vec3 Position { 0.0f };
        glm::quat Rotation = glm::identity<glm::quat>();
        glm::vec3 Scale { 1.0f };
        // The node's mesh, for its MeshRenderer: one part per "primitive", a piece of the mesh with
        // a material of its own. Empty for a node without a mesh.
        std::vector<MeshPart> Parts;
        std::vector<size_t> Children; // indices into m_Nodes
    };

    void InstantiateNode(Scene& scene, const Node& node, GameObject& parent) const;

    std::string m_Name; // the file's name without its folder and extension: "CesiumMilkTruck"
    std::vector<Node> m_Nodes;
    std::vector<size_t> m_RootNodes; // the top of the hierarchy: the nodes of the file's scene
};

} // namespace Viva
