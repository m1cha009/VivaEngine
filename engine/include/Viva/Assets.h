#pragma once

#include "Viva/Renderer.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Viva {

class Model;

// Every asset by its name: meshes, textures, materials and models. Get it from
// Application::GetAssets(). Each asset is loaded the first time it's asked for and shared after
// that, like Unity's Resources.Load. The names are what scene files write down to refer to
// assets, which is how a saved scene finds its meshes and textures again.
//
// Names:
//   Meshes    the built-in primitives "Primitives::Cube", "Primitives::Plane",
//             "Primitives::Sphere" and "Primitives::Cylinder" (see Viva/Primitives.h), sized
//             like Unity's; or one of a model's meshes, "models/CesiumMilkTruck.glb#mesh2/0": the
//             model's file, then mesh 2's primitive 0, numbered as in the file.
//   Textures  an image file in the root folder, "textures/crate.png"; or an image stored inside
//             a model, "models/CesiumMilkTruck.glb#image0".
//   Models    a .gltf or .glb file in the root folder, "models/CesiumMilkTruck.glb".
// Materials have no names: a material is its settings, so a scene file writes those instead.
class Assets {
public:
    // rootFolder: where asset names start, ending with a separator. The Application's Assets use
    // the assets folder next to the executable; a project's use the project's Assets folder (M14).
    Assets(Renderer& renderer, std::string rootFolder);
    ~Assets();

    Assets(const Assets&) = delete;
    Assets& operator=(const Assets&) = delete;

    // The built-in primitives' mesh names, for lists like the editor's mesh picker.
    static const std::vector<std::string>& GetPrimitiveNames();

    // Each returns nullptr (after logging why) if there's no such asset or it can't be loaded.
    std::shared_ptr<Mesh> GetMesh(const std::string& name);
    std::shared_ptr<Texture> GetTexture(const std::string& name);
    // A model, to place in the scene with Model::Instantiate. Loading one waits for its meshes and
    // textures to reach the GPU, so load models while loading (OnStart). Unlike the others, a model
    // stays loaded once it is, until the application ends, so instantiating it again later is quick.
    std::shared_ptr<Model> GetModel(const std::string& name);

    // A material with these settings. Asking again with equal settings returns the same material
    // while it's in use, so objects that look alike share one (and draw a little faster together).
    std::shared_ptr<Material> GetMaterial(const MaterialSettings& settings);

    Renderer& GetRenderer() const { return m_Renderer; }

    // The file an asset name stands for: the root folder plus the name.
    std::string GetFilePath(const std::string& name) const { return m_RootFolder + name; }

private:
    std::shared_ptr<Mesh> CreatePrimitive(const std::string& name);

    Renderer& m_Renderer;
    std::string m_RootFolder;

    // What's loaded. A weak_ptr remembers an asset without keeping it alive (C#'s WeakReference),
    // so an asset nobody uses any more is freed, and loaded again if it's asked for again. Models
    // are held for good: they're slow to load, and a game tends to instantiate one again and again.
    std::unordered_map<std::string, std::weak_ptr<Mesh>> m_Primitives;
    std::unordered_map<std::string, std::weak_ptr<Texture>> m_Textures;
    std::unordered_map<std::string, std::shared_ptr<Model>> m_Models;
    // The materials made here, found by comparing their settings (see Renderer::GetSettings). A
    // scene has few materials, so searching a list is quick enough.
    std::vector<std::weak_ptr<Material>> m_Materials;
};

} // namespace Viva
