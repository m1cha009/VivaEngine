# Third-party dependencies.
#
# FetchContent downloads each library at configure time into build/<preset>/_deps/ and builds it
# as part of our project, a bit like Unity's Package Manager pulling a package pinned to one
# version. Nothing gets installed system-wide. Each download is an official release archive
# checked against a SHA-256 hash, so every build gets exactly the same source code, byte for byte.
#
# SYSTEM marks a library's headers as "system" headers, so the compiler doesn't report warnings
# from inside them. Our /W4 and -Wall output then only covers our own code.

include(FetchContent)

# Let FetchContent download and unpack directly, instead of running a separate hidden CMake build
# per dependency on every configure (the old behavior, which added about 2 seconds each time).
if(POLICY CMP0168)
    cmake_policy(SET CMP0168 NEW)
endif()

# --- SDL3: window, input and (later) audio --------------------------------------------------
# Built as a static library, so it's linked straight into the executable and there's no
# SDL3.dll / libSDL3.dylib to ship next to it. The options must be set before
# FetchContent_MakeAvailable runs SDL's own CMakeLists.txt. (SDL already leaves out its tests,
# examples and install rules when it's built inside another project.)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
# Parts of SDL we never use: its own 2D renderer and GPU API (we write our own Vulkan renderer),
# and camera input. Leaving them out makes SDL quicker to build and the executable smaller.
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
    URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
    SYSTEM)

# --- GLM: vector and matrix math ------------------------------------------------------------
# GLM is all templates, so it lives entirely in headers. GLM_BUILD_LIBRARY OFF stops it from
# also compiling an optional glm.lib we don't need. glm::glm is then just "these headers".
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glm
    URL https://github.com/g-truc/glm/releases/download/1.0.3/glm-1.0.3.zip
    URL_HASH SHA256=1c0a0fced9b0d87c7b7bc94e40be490cff6d4c83c25db8488d8f33754e7fdeb2
    SYSTEM)

# --- VMA (Vulkan Memory Allocator): GPU memory management -----------------------------------
# AMD's library for allocating GPU memory, used by most Vulkan engines. Like GLM it's only
# headers: one header file, whose implementation is compiled once in
# engine/src/Renderer/VmaImplementation.cpp. Its target is GPUOpen::VulkanMemoryAllocator. (Inside
# another project it adds no install rules, so it stays out of scripts/package.cmd's output.)
FetchContent_Declare(VulkanMemoryAllocator
    URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.4.0.tar.gz
    URL_HASH SHA256=822aa850c6ce77346ae96a8a1d351d52e77e85929f35363849a0a4e638e0a2a1
    SYSTEM)

# --- stb_image: PNG and JPEG decoding --------------------------------------------------------
# Part of Sean Barrett's single-header "stb" libraries (public domain / MIT). The repository has
# no releases, so a specific commit's source archive is pinned instead. It has no CMakeLists.txt
# either: MakeAvailable just unpacks it, and the stb_image target below exposes the header. Its
# implementation is compiled once, in engine/src/Core/ImageFile.cpp.
FetchContent_Declare(stb
    URL https://github.com/nothings/stb/archive/2c980bb59875b0d32144a71867fbdebb2f77cd20.tar.gz
    URL_HASH SHA256=9a955b1b49a4410088a2e0ee2a9c057c3c907d0c1d75454144cb980aca0ba515)

# --- cgltf: glTF model loading ---------------------------------------------------------------
# A glTF parser in a single C header (MIT license). glTF is the Khronos Group's file format for
# 3D models, the one most tools export. Like stb_image it has no CMakeLists.txt at the top, so
# MakeAvailable just unpacks it, and the cgltf target below exposes the header. Its
# implementation is compiled once, in engine/src/Scene/Model.cpp.
FetchContent_Declare(cgltf
    URL https://github.com/jkuhlmann/cgltf/archive/refs/tags/v1.15.tar.gz
    URL_HASH SHA256=84e352092e5cd6aab7f66de62ddb66beb5e6f18d412ca9d12950d7a55bfef25a)

FetchContent_MakeAvailable(SDL3 glm VulkanMemoryAllocator stb cgltf)

# An INTERFACE library is a target with no code of its own, only settings for whoever links it:
# here, the folder with stb_image.h (or cgltf.h), as a SYSTEM include so its warnings stay
# hidden. The namespaced alias matches the other dependencies, and a misspelled "stb::" name
# fails when CMake configures instead of when the linker runs.
add_library(stb_image INTERFACE)
add_library(stb::image ALIAS stb_image)
target_include_directories(stb_image SYSTEM INTERFACE ${stb_SOURCE_DIR})

add_library(cgltf INTERFACE)
add_library(cgltf::cgltf ALIAS cgltf)
target_include_directories(cgltf SYSTEM INTERFACE ${cgltf_SOURCE_DIR})

# GLM's settings must be identical everywhere GLM is used, so they're attached to GLM's own
# target: anything that links glm::glm gets them.
# DEPTH_ZERO_TO_ONE: Vulkan's clip space has depth 0..1, not OpenGL's -1..1.
# RADIANS: angles in radians. GLM 1.x always does this and ignores the define, but it documents
# the intent.
target_compile_definitions(glm INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS)

# --- Vulkan: headers and loader from the LunarG Vulkan SDK ----------------------------------
# Not fetched: the SDK is installed on the machine, because it also provides the validation
# layers and the glslc shader compiler. FindVulkan locates it through the VULKAN_SDK environment
# variable (Windows, or macOS after sourcing setup-env.sh) or the system-wide install on macOS.
find_package(Vulkan REQUIRED COMPONENTS glslc)

# --- Dear ImGui: the debug UI ---------------------------------------------------------------
# An "immediate mode" GUI: code describes the UI anew every frame, like Unity's OnGUI and
# EditorGUILayout (if (ImGui::Button("Reset")) ...). Its archive has no CMakeLists.txt, so the
# targets are defined here.
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz
    URL_HASH SHA256=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99)
FetchContent_MakeAvailable(imgui)

# ImGui comes in two parts. The core (imgui.h) is what games call to build their windows. The
# backends connect it to SDL (input, window size) and to Vulkan (drawing), and only the engine
# uses them. Both are OBJECT libraries: their files are compiled with ImGui's own settings
# (without our warnings-as-errors), and the compiled objects then go into the engine library that
# links them, as if they were the engine's own files.
add_library(imgui OBJECT
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
add_library(imgui::imgui ALIAS imgui)
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR})
# The engine's public headers, for Viva/ImGuiConfig.h. A separate call because SYSTEM applies to a
# whole call, and marked SYSTEM it would hide warnings in the engine's own headers too.
target_include_directories(imgui PUBLIC ${PROJECT_SOURCE_DIR}/engine/include)
# Settings that every file including imgui.h must agree on, so they're PUBLIC.
# IMGUI_USER_CONFIG: imgui.h includes this header first. It routes ImGui's asserts to ours.
# IMGUI_DISABLE_OBSOLETE_FUNCTIONS: hides old names kept for old code, leaving the current API.
target_compile_definitions(imgui PUBLIC
    IMGUI_USER_CONFIG="Viva/ImGuiConfig.h"
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
# The same C++ version as the rest of the program (MSVC would otherwise compile it as C++14).
target_compile_features(imgui PUBLIC cxx_std_20)

add_library(imgui_backends OBJECT
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
add_library(imgui::backends ALIAS imgui_backends)
target_include_directories(imgui_backends SYSTEM PUBLIC ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_backends
    PUBLIC imgui
    PRIVATE SDL3::SDL3-static Vulkan::Vulkan)
