#include "Viva/ComponentRegistry.h"

#include "Viva/Assert.h"
#include "Viva/Camera.h"
#include "Viva/FlyCamera.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Spinner.h"

#include <vector>

namespace Viva {

namespace {

struct Entry {
    std::string Name;
    std::type_index Type;
    Component& (*Factory)(GameObject&);
};

// The registry's list. A static variable inside a function is created when the function first
// runs, so it exists before anyone uses it, whichever file asks first. (A global would be created
// at some unspecified point before main, maybe after another file's code had used it.) A handful
// of types: a linear search is enough.
std::vector<Entry>& Entries()
{
    static std::vector<Entry> s_Entries;
    return s_Entries;
}

} // namespace

// The engine's own components, registered before the registry is first used. A plain flag rather
// than a static initialized by these calls: they call back into Register, which asks again.
void ComponentRegistry::RegisterEngineComponents()
{
    static bool s_Registered = false;
    if (s_Registered)
        return;
    s_Registered = true;
    Register<MeshRenderer>("MeshRenderer");
    Register<Camera>("Camera");
    Register<Spinner>("Spinner");
    Register<FlyCamera>("FlyCamera");
}

void ComponentRegistry::Register(std::string name, std::type_index type, Factory factory)
{
    RegisterEngineComponents();
    for (const Entry& entry : Entries()) {
        if (entry.Type == type) {
            VIVA_ASSERT(entry.Name == name, "{} is registered as {} already", name, entry.Name);
            return;
        }
        if (entry.Name == name) {
            VIVA_ASSERT(false, "Two component types can't both be registered as {}", name);
            return;
        }
    }
    Entries().push_back({ std::move(name), type, factory });
}

Component* ComponentRegistry::Add(GameObject& gameObject, std::string_view name)
{
    RegisterEngineComponents();
    for (const Entry& entry : Entries()) {
        if (entry.Name == name)
            return &entry.Factory(gameObject);
    }
    return nullptr;
}

std::vector<std::string> ComponentRegistry::GetNames()
{
    RegisterEngineComponents();
    std::vector<std::string> names;
    for (const Entry& entry : Entries())
        names.push_back(entry.Name);
    return names;
}

std::string_view ComponentRegistry::NameOf(const Component& component)
{
    // typeid of a reference to a class with virtual functions gives the object's real type: a
    // Spinner's, even though `component` is a Component&.
    RegisterEngineComponents();
    const std::type_index type = typeid(component);
    for (const Entry& entry : Entries()) {
        if (entry.Type == type)
            return entry.Name;
    }
    return {};
}

} // namespace Viva
