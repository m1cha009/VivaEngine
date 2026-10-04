#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace Viva {

class Material;
class Mesh;
class Texture;
struct MaterialSettings;

// The other half of Component::VisitFields: something that does a job with each of a component's
// fields, given its name and a reference to it. One visitor writes the fields into a scene file,
// another reads them back from one, and the editor's Inspector (M16) will be a third that draws a
// widget per field. The component lists its fields once, and every job uses that list:
//
//     void Spinner::VisitFields(FieldVisitor& fields)
//     {
//         fields.Field("Axis", Axis);
//         fields.Field("DegreesPerSecond", DegreesPerSecond);
//     }
//
// Unity finds a MonoBehaviour's serialized fields by itself, through C#'s reflection: code that
// asks at runtime which fields a class has. C++ has no reflection, so each component says it here.
// This is the "visitor" pattern: the component walks its fields, and the visitor decides what
// happens at each one. Unreal Engine's Serialize(FArchive&) works the same way.
//
// A field the visitor reads may change; one it writes stays as it is. Fields missing from a file
// keep the value they have, so components get their defaults for anything a file doesn't say.
class FieldVisitor {
public:
    virtual ~FieldVisitor() = default;

    // One overload per type of field. More types come when a component needs them.
    virtual void Field(std::string_view name, float& value) = 0;
    virtual void Field(std::string_view name, glm::vec2& value) = 0;
    virtual void Field(std::string_view name, glm::vec3& value) = 0;
    virtual void Field(std::string_view name, glm::vec4& value) = 0;
    virtual void Field(std::string_view name, glm::quat& value) = 0;
    // GPU resources go into files by name (see Assets): a mesh or a texture as its asset name, a
    // material as its settings (see VisitFields below).
    virtual void Field(std::string_view name, std::shared_ptr<Mesh>& value) = 0;
    virtual void Field(std::string_view name, std::shared_ptr<Texture>& value) = 0;
    virtual void Field(std::string_view name, std::shared_ptr<Material>& value) = 0;

    // A list of elements, each with fields of its own, like MeshRenderer's parts:
    //     fields.List("Parts", Parts, [](FieldVisitor& part, MeshPart& element) {
    //         part.Field("Mesh", element.Mesh);
    //         part.Field("Material", element.Material);
    //     });
    // Reading a file resizes the list to the file's. A template, so it can take any list and any
    // lambda; virtual functions can't be templates, so it's built on the virtual Begin/End
    // functions below.
    template <typename T, typename VisitElement>
    void List(std::string_view name, std::vector<T>& list, VisitElement visitElement)
    {
        list.resize(BeginList(name, list.size()));
        for (size_t i = 0; i < list.size(); ++i) {
            BeginElement(i);
            visitElement(*this, list[i]);
            EndElement();
        }
        EndList();
    }

protected:
    // Returns how many elements the list has: `size` when writing, the file's count when reading.
    virtual size_t BeginList(std::string_view name, size_t size) = 0;
    virtual void BeginElement(size_t index) = 0;
    virtual void EndElement() = 0;
    virtual void EndList() = 0;
};

// A material's settings, field by field: what a visitor's Material overload visits inside the
// material. A material isn't a component, so this is a free function rather than an override.
void VisitFields(FieldVisitor& fields, MaterialSettings& settings);

} // namespace Viva
