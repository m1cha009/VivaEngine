#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <string_view>
#include <vector>

namespace Viva {

class FieldVisitor;
class GameObject;

// Where a GameObject is, how it's turned and how big it is, relative to its parent: Unity's
// Transform. Every GameObject has exactly one, created and destroyed with it.
//
// The local position, rotation and scale are plain fields, so code (and the debug UI) can change
// them directly. Everything in world space is computed from them when asked.
class Transform {
public:
    // The axes in an object's own space: Forward (+Z), Up (+Y) and Right (-X), as below. Rotating
    // them by an object's rotation gives the directions Forward(), Up() and Right() return.
    static constexpr glm::vec3 kForwardAxis { 0.0f, 0.0f, 1.0f };
    static constexpr glm::vec3 kUpAxis { 0.0f, 1.0f, 0.0f };
    static constexpr glm::vec3 kRightAxis { -1.0f, 0.0f, 0.0f };

    // Leaves the hierarchy: its parent forgets it, and its children become root objects. So
    // whatever order the scene destroys GameObjects in, no Transform is left pointing at one
    // that's gone.
    ~Transform();

    Transform(const Transform&) = delete;
    Transform& operator=(const Transform&) = delete;

    // Relative to the parent, or to the world for a GameObject without one: Unity's
    // localPosition, localRotation and localScale.
    glm::vec3 LocalPosition { 0.0f };
    // A rotation stored as a quaternion, like Unity's Quaternion: four numbers that describe
    // turning by an angle around an axis. Make one with glm::angleAxis(angle, axis), and combine
    // two by multiplying them. It starts as the identity: not turned at all (Unity's
    // Quaternion.identity).
    glm::quat LocalRotation = glm::identity<glm::quat>();
    glm::vec3 LocalScale { 1.0f };

    // The local values as one matrix: scale, then rotate, then move into place (applied right to
    // left, as with the model matrices of M6).
    glm::mat4 LocalMatrix() const;
    // From this object's space to the world's: its parent's world matrix times its own local
    // matrix, all the way up the hierarchy (Unity's localToWorldMatrix). Computed on every call.
    glm::mat4 WorldMatrix() const;

    // In world space: its position (Unity's position), and the directions it faces, as unit
    // vectors (Unity's forward, right and up). Forward is the object's +Z: where a model faces
    // (glTF's convention) and where a camera looks. Up is +Y. Right is -X: in a right-handed world
    // (see Camera.h), something facing +Z with +Y up has its right side towards -X. (Unity is
    // left-handed, so there it's +X.)
    glm::vec3 GetPosition() const;
    glm::vec3 Forward() const;
    glm::vec3 Right() const;
    glm::vec3 Up() const;
    // The rotation in world space (Unity's rotation): the parent's, then this one's own.
    glm::quat GetRotation() const;

    // Turns the object so its Forward points at `worldPoint`, with its Up as close to `worldUp`
    // as that allows: Unity's transform.LookAt. Looking straight along `worldUp` (or at its own
    // position) has no single answer, so then nothing changes.
    void LookAt(const glm::vec3& worldPoint, const glm::vec3& worldUp = glm::vec3(0.0f, 1.0f, 0.0f));

    // The hierarchy. A child moves, turns and scales with its parent. nullptr makes it a root
    // object. By default SetParent keeps the local values, so the object jumps to the same place
    // relative to its new parent: Unity's SetParent(parent, false). With keepWorldPose, the local
    // values change instead, so the object stays where it is in the world, as when you drag it
    // onto another in Unity's Hierarchy: SetParent(parent, true).
    Transform* GetParent() const { return m_Parent; }
    const std::vector<Transform*>& GetChildren() const { return m_Children; }
    void SetParent(Transform* parent, bool keepWorldPose = false);
    // Whether SetParent(parent) is allowed: not below itself or one of its own children, which
    // would make the hierarchy a loop. nullptr (a root object) always is.
    bool CanSetParent(const Transform* parent) const;
    // Whether `ancestor` is this transform's parent, or its parent's parent, and so on.
    bool IsBelow(const Transform& ancestor) const;

    // The child whose GameObject has this name, or nullptr: Unity's transform.Find. Only direct
    // children are searched, but a path like "Body/Wheels" goes one level down per name. Names
    // needn't be unique: the first match wins, and with a path, the first one the rest is below.
    Transform* Find(std::string_view path) const;

    GameObject& GetGameObject() const { return *m_GameObject; }

    // The local position, rotation and scale, for scene files (see Component::VisitFields). The
    // parent isn't a field: a scene file nests children inside their parent instead.
    void VisitFields(FieldVisitor& fields);

private:
    // Only a GameObject creates its Transform (see GameObject's members).
    friend class GameObject;
    explicit Transform(GameObject& gameObject);

    GameObject* m_GameObject;
    Transform* m_Parent = nullptr;
    // Not owned: every GameObject is owned by the scene, and these only point at their Transforms.
    std::vector<Transform*> m_Children;
};

// Takes a matrix built like LocalMatrix (move * turn * scale) apart again, into its position,
// rotation and scale. A matrix that also skews (a rotated child of a parent scaled unevenly
// can't be described by a position, a rotation and a scale) comes out as the nearest such
// values; Unity's lossyScale is the same compromise.
void DecomposeMatrix(const glm::mat4& matrix, glm::vec3& position, glm::quat& rotation, glm::vec3& scale);

} // namespace Viva
