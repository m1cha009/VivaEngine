#include "Viva/Model.h"

#include "Core/ImageFile.h"
#include "Viva/FileSystem.h"
#include "Viva/Assets.h"
#include "Viva/GameObject.h"
#include "Viva/Log.h"
#include "Viva/MeshData.h"
#include "Viva/Renderer.h"
#include "Viva/Scene.h"

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>

// cgltf is a single header, like stb_image: this file defines CGLTF_IMPLEMENTATION, so cgltf's
// code is compiled here, once.
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <numeric>
#include <span>

namespace Viva {

namespace {

// cgltf is a C library. It reads files through callbacks like these, and hands each block of
// memory back to be freed when it's done with it. ReadFile goes through the engine's
// ReadBinaryFile, which takes UTF-8 paths on every OS (cgltf's own reading uses C's fopen, which
// doesn't on Windows). C knows nothing of std::vector, so the bytes are copied into a block from
// std::malloc, and ReleaseFile gives it back with std::free.
cgltf_result ReadFile(const cgltf_memory_options* /*memory*/, const cgltf_file_options* /*file*/, const char* path,
                      cgltf_size* size, void** data)
{
    const std::optional<std::vector<uint8_t>> file = ReadBinaryFile(path);
    if (!file)
        return cgltf_result_file_not_found;
    // For a .bin file, cgltf passes in the size the model says it has: a shorter file would leave
    // it reading past the end of the memory.
    if (file->empty() || file->size() < *size)
        return cgltf_result_data_too_short;

    void* copy = std::malloc(file->size());
    if (!copy)
        return cgltf_result_out_of_memory;
    std::memcpy(copy, file->data(), file->size());
    *size = file->size();
    *data = copy;
    return cgltf_result_success;
}

void ReleaseFile(const cgltf_memory_options* /*memory*/, const cgltf_file_options* /*file*/, void* data)
{
    std::free(data);
}

// Frees cgltf's data when it goes out of scope. A std::unique_ptr can own memory from a C library
// too, given the function that frees it: its "deleter". It plays the role of C#'s using block.
struct CgltfDeleter {
    void operator()(cgltf_data* data) const { cgltf_free(data); }
};
using CgltfData = std::unique_ptr<cgltf_data, CgltfDeleter>;

const char* ResultName(cgltf_result result)
{
    switch (result) {
    case cgltf_result_data_too_short: return "the data is too short";
    case cgltf_result_unknown_format: return "unknown format";
    case cgltf_result_invalid_json: return "invalid JSON";
    case cgltf_result_invalid_gltf: return "invalid glTF data";
    case cgltf_result_file_not_found: return "a file is missing";
    case cgltf_result_io_error: return "a file couldn't be read";
    case cgltf_result_out_of_memory: return "out of memory";
    case cgltf_result_legacy_gltf: return "it's glTF 1.0, and only 2.0 is supported";
    default: return "unknown error";
    }
}

// Reads a .gltf or .glb file, with the .bin files it refers to, and checks it. Returns null (after
// logging why) if that fails.
CgltfData Parse(const std::string& path)
{
    cgltf_options options {};
    options.file.read = &ReadFile;
    options.file.release = &ReleaseFile;

    cgltf_data* parsed = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &parsed);
    CgltfData data(parsed); // owned from here on: freed on every return below
    if (result == cgltf_result_success)
        result = cgltf_load_buffers(&options, data.get(), path.c_str());
    // Among other things, validation checks that every index and offset in the file stays inside
    // its data, so the code below can trust them.
    if (result == cgltf_result_success)
        result = cgltf_validate(data.get());
    if (result != cgltf_result_success) {
        Log::Error("Couldn't load the model {}: {}", path, ResultName(result));
        return nullptr;
    }
    return data;
}

// Where a node sits relative to its parent, as a Transform holds it. glTF gives either the
// position, rotation and scale (each optional), or one matrix that combines them. "Out"
// parameters, like C#'s out: the function writes its results into the caller's variables.
void ReadTransform(const cgltf_node& node, glm::vec3& position, glm::quat& rotation, glm::vec3& scale)
{
    if (node.has_matrix) {
        // glTF stores a matrix column by column, as GLM does. It's taken apart into the three
        // pieces: column 3 is the position, and the first three columns are the node's axes, each
        // as long as the scale along it. (glTF only allows matrices that come apart this way.)
        const glm::mat4 matrix = glm::make_mat4(node.matrix);
        position = glm::vec3(matrix[3]);
        const glm::mat3 axes(matrix);
        scale = { glm::length(axes[0]), glm::length(axes[1]), glm::length(axes[2]) };
        // A negative determinant means the axes are mirrored, which a negative scale does.
        if (glm::determinant(axes) < 0.0f)
            scale.x = -scale.x;
        // Divided by their scales, the axes are a pure rotation, which quat_cast turns into a
        // quaternion.
        if (scale.x != 0.0f && scale.y != 0.0f && scale.z != 0.0f)
            rotation = glm::quat_cast(glm::mat3(axes[0] / scale.x, axes[1] / scale.y, axes[2] / scale.z));
        return;
    }
    if (node.has_translation)
        position = glm::make_vec3(node.translation);
    // glTF stores a quaternion as x, y, z, w. GLM's wxyz takes w first.
    if (node.has_rotation)
        rotation = glm::quat::wxyz(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
    if (node.has_scale)
        scale = glm::make_vec3(node.scale);
}

// Reads an accessor as floats. An accessor describes how to read an array out of the file's
// binary data: where it starts, how many elements, and what each one is (three floats for a
// position, for example). cgltf converts what the file stores, floats or integers scaled to
// 0..1, into floats.
std::vector<float> ReadFloats(const cgltf_accessor& accessor)
{
    std::vector<float> values(accessor.count * cgltf_num_components(accessor.type));
    cgltf_accessor_unpack_floats(&accessor, values.data(), values.size());
    return values;
}

// Turns a primitive (a piece of a mesh, drawn with one material) into the engine's MeshData: the
// positions, texture coordinates and colors of its vertices, and its triangles. The other
// attributes a file can have (normals and tangents for lighting, weights for skinning) aren't used
// yet. Returns std::nullopt (after logging why) for a primitive the engine can't draw.
std::optional<MeshData> ReadPrimitive(const cgltf_primitive& primitive, const std::string& meshName)
{
    if (primitive.type != cgltf_primitive_type_triangles) {
        Log::Warn("Skipping a part of the mesh {}: it isn't made of triangles", meshName);
        return std::nullopt;
    }
    const cgltf_accessor* positions = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    if (!positions || positions->type != cgltf_type_vec3) {
        Log::Warn("Skipping a part of the mesh {}: it has no vertex positions", meshName);
        return std::nullopt;
    }

    // Every attribute has one element per vertex (cgltf_validate checked). Vertices start white
    // with UV (0, 0), for primitives that have no colors or texture coordinates.
    MeshData mesh;
    mesh.Vertices.resize(positions->count,
                         { .Position = glm::vec3(0.0f), .Color = glm::vec3(1.0f), .UV = glm::vec2(0.0f) });
    const std::vector<float> position = ReadFloats(*positions);
    for (size_t i = 0; i < mesh.Vertices.size(); ++i)
        mesh.Vertices[i].Position = glm::make_vec3(&position[i * 3]);

    // Texture coordinates: glTF puts (0, 0) at the image's top-left corner, as the engine does
    // (see Vertex), so they're used as they are. Only the first set is read.
    const cgltf_accessor* uvs = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);
    if (uvs && uvs->type == cgltf_type_vec2) {
        const std::vector<float> uv = ReadFloats(*uvs);
        for (size_t i = 0; i < mesh.Vertices.size(); ++i)
            mesh.Vertices[i].UV = glm::make_vec2(&uv[i * 2]);
    }

    // Vertex colors, with or without alpha (which isn't used).
    const cgltf_accessor* colors = cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0);
    if (colors && (colors->type == cgltf_type_vec3 || colors->type == cgltf_type_vec4)) {
        const size_t components = cgltf_num_components(colors->type);
        const std::vector<float> color = ReadFloats(*colors);
        for (size_t i = 0; i < mesh.Vertices.size(); ++i)
            mesh.Vertices[i].Color = glm::make_vec3(&color[i * components]);
    }

    // The triangles. Without an index list, every three vertices in a row make one.
    if (primitive.indices) {
        mesh.Indices.resize(primitive.indices->count);
        const size_t read =
            cgltf_accessor_unpack_indices(primitive.indices, mesh.Indices.data(), sizeof(uint32_t), mesh.Indices.size());
        if (read != mesh.Indices.size()) {
            Log::Warn("Skipping a part of the mesh {}: its indices can't be read", meshName);
            return std::nullopt;
        }
    } else {
        mesh.Indices.resize(mesh.Vertices.size());
        std::iota(mesh.Indices.begin(), mesh.Indices.end(), 0u); // 0, 1, 2, 3...
    }
    return mesh;
}

// The texture for one of the file's images. An image is either stored inside the model's binary
// data (always, in a .glb), or a file of its own next to a .gltf, which `folder` leads to.
// Returns null (after logging why) if it can't be loaded.
std::shared_ptr<Texture> TextureFromImage(Assets& assets, const cgltf_data& data, const cgltf_image& image,
                                          const std::string& folder, const std::string& assetName)
{
    if (image.buffer_view) {
        const uint8_t* bytes = cgltf_buffer_view_data(image.buffer_view);
        if (!bytes) {
            Log::Error("{} has an image whose data wasn't loaded", assetName);
            return nullptr;
        }
        const std::optional<ImageData> decoded =
            DecodeImage(std::span(bytes, image.buffer_view->size), std::format("an image in {}", assetName));
        if (!decoded)
            return nullptr;
        // Named after its place in the file, the way scene files will refer to it.
        return assets.GetRenderer().CreateTexture(decoded->Width, decoded->Height, decoded->Pixels,
                                                  std::format("{}#image{}", assetName, cgltf_image_index(&data, &image)));
    }

    if (!image.uri) {
        Log::Error("{} has an image without any data", assetName);
        return nullptr;
    }
    // "data:" URIs hold the whole image as text inside the JSON.
    if (std::strncmp(image.uri, "data:", 5) == 0) {
        Log::Error("{} has an image stored as a data: URI, which isn't supported", assetName);
        return nullptr;
    }
    // A file name in a "URI" may have characters escaped (a space becomes "%20"), which
    // cgltf_decode_uri undoes in place.
    std::string file = image.uri;
    file.resize(cgltf_decode_uri(file.data()));
    return assets.GetTexture(folder + file);
}

// The file's materials, in the file's order. Only their base color is used: a color and a
// texture, multiplied as in the Unlit shader. The rest of a glTF material (metalness, roughness,
// normal maps, emission) describes how light reacts, and the renderer has no lighting yet.
std::vector<std::shared_ptr<Material>> LoadMaterials(Assets& assets, const cgltf_data& data,
                                                     const std::string& folder, const std::string& assetName)
{
    // One texture per image, made when a material first uses it. The truck's body and wheels
    // share one image, so it's decoded and uploaded once.
    std::vector<std::shared_ptr<Texture>> textures(data.images_count);

    std::vector<std::shared_ptr<Material>> materials;
    for (const cgltf_material& material : std::span(data.materials, data.materials_count)) {
        const cgltf_pbr_metallic_roughness& pbr = material.pbr_metallic_roughness;
        MaterialSettings settings { .Color = glm::make_vec4(pbr.base_color_factor) };
        const cgltf_texture* texture = pbr.base_color_texture.texture;
        if (texture && texture->image) {
            std::shared_ptr<Texture>& loaded = textures[cgltf_image_index(&data, texture->image)];
            if (!loaded)
                loaded = TextureFromImage(assets, data, *texture->image, folder, assetName);
            settings.Texture = loaded;
        }
        materials.push_back(assets.GetMaterial(settings));
    }
    return materials;
}

// The file's meshes, in the file's order, each as a list of parts: one per primitive, with its
// material. Each primitive's mesh is named after its place in the file, the way scene files will
// refer to it: "models/CesiumMilkTruck.glb#mesh2/0" is mesh 2's primitive 0.
std::vector<std::vector<MeshPart>> LoadMeshes(Assets& assets, const cgltf_data& data,
                                              const std::vector<std::shared_ptr<Material>>& materials,
                                              const std::string& assetName)
{
    std::vector<std::vector<MeshPart>> meshes;
    for (size_t i = 0; i < data.meshes_count; ++i) {
        const cgltf_mesh& mesh = data.meshes[i];
        const std::string meshName = mesh.name ? mesh.name : std::format("number {}", i);
        std::vector<MeshPart>& parts = meshes.emplace_back();
        for (size_t p = 0; p < mesh.primitives_count; ++p) {
            const cgltf_primitive& primitive = mesh.primitives[p];
            const std::optional<MeshData> meshData = ReadPrimitive(primitive, meshName);
            if (!meshData)
                continue;
            // Without a material, glTF's default: plain white.
            std::shared_ptr<Material> material = primitive.material
                                                     ? materials[cgltf_material_index(&data, primitive.material)]
                                                     : assets.GetMaterial({});
            std::shared_ptr<Mesh> gpuMesh =
                assets.GetRenderer().CreateMesh(*meshData, std::format("{}#mesh{}/{}", assetName, i, p));
            if (gpuMesh && material)
                parts.push_back({ std::move(gpuMesh), std::move(material) });
        }
    }
    return meshes;
}

// A node's name, for its GameObject. Names are optional in glTF: a nameless node is called after
// its mesh, or else "Node" and its number in the file.
std::string NodeName(const cgltf_node& node, size_t index)
{
    if (node.name)
        return node.name;
    if (node.mesh && node.mesh->name)
        return node.mesh->name;
    return std::format("Node {}", index);
}

} // namespace

std::unique_ptr<Model> Model::Load(Assets& assets, const std::string& assetName)
{
    const CgltfData data = Parse(assets.GetFilePath(assetName));
    if (!data)
        return nullptr;

    // A file lists the glTF extensions it can't be read without: compressed meshes, other image
    // formats and so on. The engine supports none yet.
    for (const char* extension : std::span(data->extensions_required, data->extensions_required_count))
        Log::Error("The model {} needs the glTF extension {}, which isn't supported", assetName, extension);
    if (data->extensions_required_count > 0)
        return nullptr;

    // For "models/BoxTextured/BoxTextured.gltf", the folder is "models/BoxTextured/" (image files
    // are found relative to it) and the name "BoxTextured".
    const size_t slash = assetName.rfind('/');
    const std::string folder = slash == std::string::npos ? "" : assetName.substr(0, slash + 1);
    const std::string fileName = assetName.substr(folder.size());

    auto model = std::make_unique<Model>();
    model->m_Name = fileName.substr(0, fileName.rfind('.'));

    // The materials first (with the textures they use), then the meshes that use them.
    const std::vector<std::shared_ptr<Material>> materials = LoadMaterials(assets, *data, folder, assetName);
    const std::vector<std::vector<MeshPart>> meshes = LoadMeshes(assets, *data, materials, assetName);

    // The nodes keep the file's numbering, so a node's children are the same numbers as in the
    // file. Nodes that show the same mesh (the truck's two axles) get copies of its part list:
    // copies of the shared_ptrs, so they draw the same GPU meshes.
    for (size_t i = 0; i < data->nodes_count; ++i) {
        const cgltf_node& source = data->nodes[i];
        Node& node = model->m_Nodes.emplace_back();
        node.Name = NodeName(source, i);
        ReadTransform(source, node.Position, node.Rotation, node.Scale);
        if (source.mesh)
            node.Parts = meshes[cgltf_mesh_index(data.get(), source.mesh)];
        for (const cgltf_node* child : std::span(source.children, source.children_count))
            node.Children.push_back(cgltf_node_index(data.get(), child));
    }

    // The top of the hierarchy: the nodes of the scene the file names as its main one, or else of
    // its first scene. A file without scenes is, by the glTF spec, a library of meshes and
    // materials with nothing placed, so its instances are empty.
    const cgltf_scene* scene = data->scene;
    if (!scene && data->scenes_count > 0)
        scene = &data->scenes[0];
    if (scene) {
        for (const cgltf_node* node : std::span(scene->nodes, scene->nodes_count))
            model->m_RootNodes.push_back(cgltf_node_index(data.get(), node));
    } else {
        Log::Warn("The model {} has no scene, so its instances will be empty", assetName);
    }

    // The milk truck has an animation that turns its wheels, for example. Such data is left out.
    if (data->animations_count > 0 || data->skins_count > 0)
        Log::Info("The model {} has animations or skinning, which aren't supported yet and were left out", assetName);

    Log::Trace("Model loaded: {} ({} nodes, {} meshes, {} materials, {} images)", assetName, data->nodes_count,
               data->meshes_count, data->materials_count, data->images_count);
    return model;
}

// Both search the nodes' parts, where every mesh and material the model loaded ends up. A model
// has tens of parts at most, and this only runs while a scene file loads.

std::shared_ptr<Mesh> Model::FindMesh(const std::string& assetName) const
{
    for (const Node& node : m_Nodes) {
        for (const MeshPart& part : node.Parts) {
            if (Renderer::GetAssetName(*part.Mesh) == assetName)
                return part.Mesh;
        }
    }
    Log::Error("The model {} has no mesh {}", m_Name, assetName);
    return nullptr;
}

std::shared_ptr<Texture> Model::FindTexture(const std::string& assetName) const
{
    for (const Node& node : m_Nodes) {
        for (const MeshPart& part : node.Parts) {
            const std::shared_ptr<Texture>& texture = Renderer::GetSettings(*part.Material).Texture;
            if (texture && Renderer::GetAssetName(*texture) == assetName)
                return texture;
        }
    }
    Log::Error("The model {} has no texture {}", m_Name, assetName);
    return nullptr;
}

GameObject& Model::Instantiate(Scene& scene, GameObject* parent) const
{
    GameObject& root = scene.CreateGameObject(m_Name, parent);
    for (const size_t node : m_RootNodes)
        InstantiateNode(scene, m_Nodes[node], root);
    return root;
}

void Model::InstantiateNode(Scene& scene, const Node& node, GameObject& parent) const
{
    GameObject& gameObject = scene.CreateGameObject(node.Name, &parent);
    Transform& transform = gameObject.GetTransform();
    transform.LocalPosition = node.Position;
    transform.LocalRotation = node.Rotation;
    transform.LocalScale = node.Scale;

    // The MeshRenderer gets a copy of the part list: copies of the shared_ptrs, so every instance
    // draws the same meshes and materials on the GPU.
    if (!node.Parts.empty())
        gameObject.AddComponent<MeshRenderer>(node.Parts);

    // The function calls itself for each child ("recursion"), which builds the whole subtree.
    for (const size_t child : node.Children)
        InstantiateNode(scene, m_Nodes[child], gameObject);
}

} // namespace Viva
