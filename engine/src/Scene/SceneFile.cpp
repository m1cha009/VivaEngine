// Scene::Save and Scene::Load: scene files (and Serialize, Deserialize and Instantiate, which work
// through the same JSON). A class's member functions can be defined in more than one .cpp file; these live apart from Scene.cpp because they're a topic of their own, and they
// bring in JSON, the asset names and the component registry, which the rest of Scene doesn't need.

#include "Viva/Scene.h"

#include "Viva/Json.h"
#include "Viva/FileSystem.h"
#include "Viva/Assets.h"
#include "Viva/ComponentRegistry.h"
#include "Viva/FieldVisitor.h"
#include "Viva/Log.h"
#include "Viva/Renderer.h"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

namespace Viva {

namespace {

// The first members of every scene file: what it is, and which version of the format. A file
// from a newer engine, which may hold things this one doesn't know, is refused.
constexpr const char* kFormatName = "VivaEngine Scene";
constexpr double kFormatVersion = 1.0;

// Floats go into JSON through FloatToJsonNumber, so they're written as short as they can be.
Json Floats(std::initializer_list<float> values)
{
    Json::Array array;
    for (const float value : values)
        array.push_back(FloatToJsonNumber(value));
    return array;
}

// Writes the fields a component (or a Transform) hands over into a JSON object. `owner` says
// whose fields they are, for warnings: "Crate's MeshRenderer".
class JsonFieldWriter final : public FieldVisitor {
public:
    JsonFieldWriter(Json::Object& object, std::string owner)
        : m_Owner(std::move(owner))
    {
        m_Objects.push_back(&object);
    }

    void Field(std::string_view name, float& value) override { Add(name, FloatToJsonNumber(value)); }
    void Field(std::string_view name, glm::vec2& value) override { Add(name, Floats({ value.x, value.y })); }
    void Field(std::string_view name, glm::vec3& value) override { Add(name, Floats({ value.x, value.y, value.z })); }
    void Field(std::string_view name, glm::vec4& value) override
    {
        Add(name, Floats({ value.x, value.y, value.z, value.w }));
    }
    // x, y, z, w, the order glTF uses (GLM's own order isn't the same everywhere).
    void Field(std::string_view name, glm::quat& value) override
    {
        Add(name, Floats({ value.x, value.y, value.z, value.w }));
    }

    // Meshes and textures are written as their asset names. One made in code has none: there's
    // nothing in the file it could be loaded from again, so it's written as null.
    void Field(std::string_view name, std::shared_ptr<Mesh>& value) override
    {
        AddAssetName(name, value ? &Renderer::GetAssetName(*value) : nullptr, "mesh");
    }
    void Field(std::string_view name, std::shared_ptr<Texture>& value) override
    {
        AddAssetName(name, value ? &Renderer::GetAssetName(*value) : nullptr, "texture");
    }

    // A material is written as an object holding its settings.
    void Field(std::string_view name, std::shared_ptr<Material>& value) override
    {
        if (!value) {
            Add(name, Json());
            return;
        }
        Add(name, Json::Object());
        m_Objects.push_back(Current().back().second.AsObject());
        MaterialSettings settings = Renderer::GetSettings(*value); // a copy: VisitFields takes a reference it may change
        VisitFields(*this, settings);
        m_Objects.pop_back();
    }

protected:
    // A list is a JSON array with an object per element. The pointers in m_Objects point into the
    // JSON being built; each is used only while nothing is added to the array or object holding it.
    size_t BeginList(std::string_view name, size_t size) override
    {
        Add(name, Json::Array());
        return size;
    }
    void BeginElement(size_t /*index*/) override
    {
        // The list is the last member of the current object: BeginList just added it, and each
        // element's fields go into the element's own object.
        Json::Array& list = *Current().back().second.AsArray();
        list.push_back(Json::Object());
        m_Objects.push_back(list.back().AsObject());
    }
    void EndElement() override { m_Objects.pop_back(); }
    void EndList() override {}

private:
    Json::Object& Current() { return *m_Objects.back(); }
    void Add(std::string_view name, Json value) { Current().emplace_back(std::string(name), std::move(value)); }

    // assetName: null for no resource at all.
    void AddAssetName(std::string_view name, const std::string* assetName, std::string_view kind)
    {
        if (assetName && assetName->empty())
            Log::Warn("{} uses a {} made in code, which a scene file can't refer to: it's saved without it", m_Owner, kind);
        Add(name, assetName && !assetName->empty() ? Json(*assetName) : Json());
    }

    std::string m_Owner;
    std::vector<Json::Object*> m_Objects; // where fields go: the innermost list element or material, or the component
};

// Reads fields back from a JSON object. A field the object doesn't have keeps its value; one of
// the wrong type does too, with a warning.
class JsonFieldReader final : public FieldVisitor {
public:
    // `object` may be null (the file has no such object): then every field keeps its value.
    JsonFieldReader(const Json* object, Assets& assets, std::string owner)
        : m_Assets(assets)
        , m_Owner(std::move(owner))
    {
        m_Objects.push_back(object);
    }

    void Field(std::string_view name, float& value) override
    {
        const Json* json = Find(name);
        if (!json)
            return;
        if (const double* number = json->AsNumber())
            value = static_cast<float>(*number);
        else
            WrongType(name, "a number");
    }
    void Field(std::string_view name, glm::vec2& value) override { ReadFloats(name, &value.x, 2); }
    void Field(std::string_view name, glm::vec3& value) override { ReadFloats(name, &value.x, 3); }
    void Field(std::string_view name, glm::vec4& value) override { ReadFloats(name, &value.x, 4); }
    void Field(std::string_view name, glm::quat& value) override
    {
        float xyzw[4] = { value.x, value.y, value.z, value.w };
        if (!ReadFloats(name, xyzw, 4))
            return;
        const glm::quat read = glm::quat::wxyz(xyzw[3], xyzw[0], xyzw[1], xyzw[2]);
        // A rotation must be a quaternion of length 1. A file edited by hand may say [0, 1, 0, 1];
        // normalizing makes that the rotation it means. All zeros means nothing at all. One that's
        // as close to 1 as floats get is kept exactly as written: normalizing it would change its
        // last digits, and saving the scene again would then rewrite every rotation.
        const float length = glm::length(read);
        if (length == 0.0f) {
            Log::Warn("{}: {} isn't a rotation", m_Owner, name);
            return;
        }
        value = std::abs(length - 1.0f) < 1e-5f ? read : read / length;
    }

    void Field(std::string_view name, std::shared_ptr<Mesh>& value) override
    {
        if (const std::optional<std::string> assetName = ReadAssetName(name, "a mesh's asset name"))
            value = assetName->empty() ? nullptr : m_Assets.GetMesh(*assetName);
    }
    void Field(std::string_view name, std::shared_ptr<Texture>& value) override
    {
        if (const std::optional<std::string> assetName = ReadAssetName(name, "a texture's asset name"))
            value = assetName->empty() ? nullptr : m_Assets.GetTexture(*assetName);
    }

    void Field(std::string_view name, std::shared_ptr<Material>& value) override
    {
        const Json* json = Find(name);
        if (!json)
            return;
        if (json->IsNull()) {
            value = nullptr;
            return;
        }
        if (!json->IsObject()) {
            WrongType(name, "an object with the material's settings");
            return;
        }
        // The settings, then the material: Assets shares one material between equal settings.
        MaterialSettings settings;
        m_Objects.push_back(json);
        VisitFields(*this, settings);
        m_Objects.pop_back();
        value = m_Assets.GetMaterial(settings);
    }

protected:
    size_t BeginList(std::string_view name, size_t size) override
    {
        const Json* json = Find(name);
        const Json::Array* list = json ? json->AsArray() : nullptr;
        if (json && !list)
            WrongType(name, "a list");
        m_Lists.push_back(list);
        // Without the list in the file, the list keeps its elements, and each element its fields.
        return list ? list->size() : size;
    }
    void BeginElement(size_t index) override
    {
        const Json::Array* list = m_Lists.back();
        m_Objects.push_back(list ? &(*list)[index] : nullptr);
    }
    void EndElement() override { m_Objects.pop_back(); }
    void EndList() override { m_Lists.pop_back(); }

private:
    // The current object's member `name`, or nullptr. A list element that isn't an object has no
    // members, so its fields keep their values.
    const Json* Find(std::string_view name) const
    {
        const Json* object = m_Objects.back();
        return object ? object->Find(name) : nullptr;
    }

    // Reads an array of `count` numbers into `values`. Returns whether it did.
    bool ReadFloats(std::string_view name, float* values, size_t count)
    {
        const Json* json = Find(name);
        if (!json)
            return false;
        const Json::Array* array = json->AsArray();
        bool valid = array && array->size() == count;
        for (size_t i = 0; valid && i < count; ++i)
            valid = (*array)[i].IsNumber();
        if (!valid) {
            WrongType(name, std::format("a list of {} numbers", count));
            return false;
        }
        for (size_t i = 0; i < count; ++i)
            values[i] = static_cast<float>(*(*array)[i].AsNumber());
        return true;
    }

    // An asset name, "" for null (no resource), or std::nullopt if there's nothing to read: the
    // field is missing, or isn't a string.
    std::optional<std::string> ReadAssetName(std::string_view name, std::string_view expected) const
    {
        const Json* json = Find(name);
        if (!json)
            return std::nullopt;
        if (json->IsNull())
            return std::string();
        if (const std::string* assetName = json->AsString())
            return *assetName;
        WrongType(name, expected);
        return std::nullopt;
    }

    void WrongType(std::string_view name, std::string_view expected) const
    {
        Log::Warn("{}: {} should be {}; it keeps its value", m_Owner, name, expected);
    }

    Assets& m_Assets;
    std::string m_Owner;
    std::vector<const Json*> m_Objects;
    std::vector<const Json::Array*> m_Lists;
};

Json SaveGameObject(const GameObject& gameObject, bool withIds)
{
    Json::Object object;
    object.emplace_back("Name", gameObject.GetName());
    if (withIds)
        object.emplace_back("Id", static_cast<double>(gameObject.GetId())); // exact up to 2^53
    object.emplace_back("Active", gameObject.IsActiveSelf());

    // VisitFields isn't const, because reading changes the fields. Writing doesn't, and the scene
    // owns these objects, so the const_casts are safe: they only lift "const" so the same
    // function serves both directions.
    Json::Object transform;
    JsonFieldWriter transformWriter(transform, gameObject.GetName());
    const_cast<GameObject&>(gameObject).GetTransform().VisitFields(transformWriter);
    object.emplace_back("Transform", std::move(transform));

    Json::Array components;
    for (const std::unique_ptr<Component>& component : gameObject.GetComponents()) {
        if (component->IsDestroyed())
            continue; // removed this frame (Scene::Destroy), as good as gone
        const std::string_view type = ComponentRegistry::NameOf(*component);
        if (type.empty()) {
            // typeid(...).name() is the compiler's name for the type, "class TruckController" on
            // MSVC: good enough to say which one.
            const Component& unregistered = *component;
            Log::Warn("{}: its component {} isn't registered, so it isn't saved (see ComponentRegistry)",
                      gameObject.GetName(), typeid(unregistered).name());
            continue;
        }
        Json::Object fields;
        fields.emplace_back("Type", std::string(type));
        JsonFieldWriter writer(fields, std::format("{}'s {}", gameObject.GetName(), type));
        component->VisitFields(writer);
        components.push_back(std::move(fields));
    }
    if (!components.empty())
        object.emplace_back("Components", std::move(components));

    // Children are nested inside their parent, so the file reads like the Hierarchy window.
    Json::Array children;
    for (const Transform* child : gameObject.GetTransform().GetChildren()) {
        if (!child->GetGameObject().IsDestroyed())
            children.push_back(SaveGameObject(child->GetGameObject(), withIds));
    }
    if (!children.empty())
        object.emplace_back("Children", std::move(children));
    return object;
}

} // namespace

GameObject* Scene::LoadGameObject(const Json& json, GameObject* parent, Assets& assets)
{
    if (!json.IsObject()) {
        Log::Warn("Skipping a GameObject that isn't a JSON object");
        return nullptr;
    }
    const Json* name = json.Find("Name");
    GameObject& gameObject = CreateGameObject(name && name->AsString() ? *name->AsString() : "GameObject", parent);
    // A snapshot's Id (see Serialize) replaces the new one; later objects are numbered past it.
    if (const Json* id = json.Find("Id"); id && id->AsNumber()) {
        gameObject.m_Id = static_cast<uint64_t>(*id->AsNumber());
        m_NextId = std::max(m_NextId, gameObject.m_Id + 1);
    }
    if (const Json* active = json.Find("Active"); active && active->AsBool())
        gameObject.SetActive(*active->AsBool());

    JsonFieldReader transformReader(json.Find("Transform"), assets, gameObject.GetName());
    gameObject.GetTransform().VisitFields(transformReader);

    if (const Json* components = json.Find("Components"); components && components->AsArray()) {
        for (const Json& fields : *components->AsArray()) {
            const Json* type = fields.Find("Type");
            if (!type || !type->AsString()) {
                Log::Warn("{}: skipping a component without a type", gameObject.GetName());
                continue;
            }
            Component* component = ComponentRegistry::Add(gameObject, *type->AsString());
            if (!component) {
                Log::Warn("{}: no component type is registered as {}, so it's left out (see ComponentRegistry)",
                          gameObject.GetName(), *type->AsString());
                continue;
            }
            JsonFieldReader reader(&fields, assets, std::format("{}'s {}", gameObject.GetName(), *type->AsString()));
            component->VisitFields(reader);
        }
    }

    if (const Json* children = json.Find("Children"); children && children->AsArray()) {
        for (const Json& child : *children->AsArray())
            LoadGameObject(child, &gameObject, assets);
    }
    return &gameObject;
}

Json Scene::Serialize(bool withIds) const
{
    // The roots, in the order they were created; each brings its children along.
    Json::Array gameObjects;
    for (const std::unique_ptr<GameObject>& gameObject : m_GameObjects) {
        if (!gameObject->GetTransform().GetParent() && !gameObject->IsDestroyed())
            gameObjects.push_back(SaveGameObject(*gameObject, withIds));
    }

    Json::Object file;
    file.emplace_back("Format", kFormatName);
    file.emplace_back("Version", kFormatVersion);
    file.emplace_back("GameObjects", std::move(gameObjects));
    return file;
}

bool Scene::Save(const std::string& path) const
{
    if (!WriteTextFile(path, WriteJson(Serialize())))
        return false;
    Log::Info("Scene saved: {}", path);
    return true;
}

GameObject& Scene::Instantiate(const GameObject& original, Assets& assets, GameObject* parent)
{
    // Into JSON and straight back out, without a file in between. Saving always gives an object,
    // so loading it always makes one.
    return *LoadGameObject(SaveGameObject(original, false), parent, assets);
}

bool Scene::Load(const std::string& path, Assets& assets)
{
    std::string error;
    const std::optional<Json> file = ReadJsonFile(path, kFormatName, kFormatVersion, error);
    if (!file) {
        Log::Error("{}", error);
        return false;
    }
    // Every GameObject created is added to the end of m_GameObjects, so the growth is the count.
    const size_t before = m_GameObjects.size();
    if (!Deserialize(*file, assets, path))
        return false;
    Log::Info("Scene loaded: {} ({} GameObjects)", path, m_GameObjects.size() - before);
    return true;
}

bool Scene::Deserialize(const Json& json, Assets& assets, std::string_view source)
{
    const Json* gameObjects = json.Find("GameObjects");
    if (!gameObjects || !gameObjects->AsArray()) {
        Log::Error("{} has no list of GameObjects", source);
        return false;
    }
    for (const Json& gameObject : *gameObjects->AsArray())
        LoadGameObject(gameObject, nullptr, assets);
    return true;
}

} // namespace Viva
