#pragma once

#include "Viva/GameObject.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>

namespace Viva {

// The component types that scene files can hold, each under a name: "Spinner" in a file means a
// Spinner. Loading a scene looks the name up and adds that type of component; saving one looks a
// component's type up and writes its name.
//
// Unity needs nothing like this: it finds every MonoBehaviour class through C#'s reflection, and
// a scene refers to a script by its GUID. C++ can't list its classes at runtime, so each type is
// registered once, before any scene is loaded or saved. The engine's own components (MeshRenderer,
// Camera, Spinner, FlyCamera) are registered already; a game registers its own, typically in
// OnStart:
//
//     ComponentRegistry::Register<TruckWheels>("TruckWheels");
//
// Why not have every component register itself, from a global variable in its .cpp file? A static
// library's code only reaches the executable if something in the game uses it, so such a file
// could be left out entirely, its registration with it. Registering by hand avoids that trap.
//
// A static class, like Input and Time: there's one registry for the whole program.
class ComponentRegistry {
public:
    ComponentRegistry() = delete;

    // Registers T under `name`. T needs a constructor without arguments, which loading uses.
    // Registering the same type again does nothing.
    template <typename T>
    static void Register(std::string name)
    {
        static_assert(std::is_base_of_v<Component, T>, "Register<T>: T must derive from Viva::Component");
        static_assert(std::is_default_constructible_v<T>, "Register<T>: T needs a constructor without arguments");
        // &Create<T> points at the version of Create for T: the factory that loading calls.
        Register(std::move(name), typeid(T), &Create<T>);
    }

    // Adds a component of the type registered as `name` to the GameObject, or returns nullptr if
    // no type has that name.
    static Component* Add(GameObject& gameObject, std::string_view name);

    // The name a component's type was registered with, or "" if it wasn't registered.
    static std::string_view NameOf(const Component& component);

private:
    // A pointer to a function that adds a component to a GameObject: a factory.
    using Factory = Component& (*)(GameObject&);
    template <typename T>
    static Component& Create(GameObject& gameObject)
    {
        return gameObject.AddComponent<T>();
    }
    // typeid(T) identifies a type at runtime. std::type_index wraps it so it can be compared and
    // stored, like C#'s System.Type.
    static void Register(std::string name, std::type_index type, Factory factory);
    static void RegisterEngineComponents();
};

} // namespace Viva
