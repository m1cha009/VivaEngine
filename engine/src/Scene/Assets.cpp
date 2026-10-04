#include "Viva/Assets.h"

#include "Core/ImageFile.h"
#include "Viva/FileSystem.h"
#include "Viva/Log.h"
#include "Viva/Model.h"
#include "Viva/Primitives.h"

#include <optional>
#include <utility>

namespace Viva {

namespace {

// For a name inside a model, "models/CesiumMilkTruck.glb#image0": the model's own name, the part
// before the '#'. std::nullopt for any other name.
std::optional<std::string> ModelOf(const std::string& name)
{
    const size_t hash = name.find('#');
    if (hash == std::string::npos)
        return std::nullopt;
    return name.substr(0, hash);
}

} // namespace

Assets::Assets(Renderer& renderer, std::string rootFolder)
    : m_Renderer(renderer)
    , m_RootFolder(std::move(rootFolder))
{
}

// Defined here, where Model is a complete type: destroying the models needs its destructor.
Assets::~Assets() = default;

std::shared_ptr<Mesh> Assets::GetMesh(const std::string& name)
{
    if (name.starts_with("Primitives::")) {
        // lock() turns the weak_ptr into a shared_ptr, or null if the mesh is gone.
        std::weak_ptr<Mesh>& cached = m_Primitives[name];
        std::shared_ptr<Mesh> mesh = cached.lock();
        if (!mesh) {
            mesh = CreatePrimitive(name);
            cached = mesh;
        }
        return mesh;
    }
    if (const std::optional<std::string> modelName = ModelOf(name)) {
        const std::shared_ptr<Model> model = GetModel(*modelName);
        return model ? model->FindMesh(name) : nullptr;
    }
    Log::Error("There's no mesh called {}", name);
    return nullptr;
}

namespace {

// The built-in primitives: each name, and the function that builds its mesh (a pointer to a
// function, like a C# Func<MeshData>). The one list of them: a new primitive is one more line.
struct Primitive {
    const char* Name;
    MeshData (*Build)();
};
constexpr Primitive kPrimitives[] = {
    { "Primitives::Cube", &Primitives::Cube },
    { "Primitives::Plane", &Primitives::Plane },
    { "Primitives::Sphere", &Primitives::Sphere },
    { "Primitives::Cylinder", &Primitives::Cylinder },
};

} // namespace

const std::vector<std::string>& Assets::GetPrimitiveNames()
{
    static const std::vector<std::string> s_Names = [] {
        std::vector<std::string> names;
        for (const Primitive& primitive : kPrimitives)
            names.emplace_back(primitive.Name);
        return names;
    }();
    return s_Names;
}

std::shared_ptr<Mesh> Assets::CreatePrimitive(const std::string& name)
{
    for (const Primitive& primitive : kPrimitives) {
        if (name == primitive.Name)
            return m_Renderer.CreateMesh(primitive.Build(), name);
    }
    Log::Error("There's no primitive called {}", name);
    return nullptr;
}

std::shared_ptr<Texture> Assets::GetTexture(const std::string& name)
{
    if (const std::optional<std::string> modelName = ModelOf(name)) {
        const std::shared_ptr<Model> model = GetModel(*modelName);
        return model ? model->FindTexture(name) : nullptr;
    }

    std::weak_ptr<Texture>& cached = m_Textures[name];
    if (std::shared_ptr<Texture> texture = cached.lock())
        return texture;

    // The file is decoded on the CPU (stb_image, M7), and the pixels are uploaded like any others.
    const std::optional<ImageData> image = LoadImageFile(GetFilePath(name));
    if (!image)
        return nullptr;
    std::shared_ptr<Texture> texture = m_Renderer.CreateTexture(image->Width, image->Height, image->Pixels, name);
    Log::Trace("Texture loaded: {} ({}x{})", name, image->Width, image->Height);
    cached = texture;
    return texture;
}

std::shared_ptr<Model> Assets::GetModel(const std::string& name)
{
    // A failed load is remembered too (as null), so a missing model is reported once, not for
    // every mesh a scene file asks of it.
    if (const auto found = m_Models.find(name); found != m_Models.end())
        return found->second;
    std::shared_ptr<Model> model = Model::Load(*this, name);
    m_Models[name] = model;
    return model;
}

std::shared_ptr<Material> Assets::GetMaterial(const MaterialSettings& settings)
{
    // Forget the materials nobody uses any more, then look among the rest.
    std::erase_if(m_Materials, [](const std::weak_ptr<Material>& material) { return material.expired(); });
    for (const std::weak_ptr<Material>& cached : m_Materials) {
        std::shared_ptr<Material> material = cached.lock();
        if (material && Renderer::GetSettings(*material) == settings)
            return material;
    }

    std::shared_ptr<Material> material = m_Renderer.CreateMaterial(settings);
    if (material)
        m_Materials.push_back(material);
    return material;
}

} // namespace Viva
