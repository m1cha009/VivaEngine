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

FetchContent_MakeAvailable(SDL3 glm VulkanMemoryAllocator stb)

# An INTERFACE library is a target with no code of its own, only settings for whoever links it:
# here, the folder with stb_image.h, as a SYSTEM include so its warnings stay hidden. The
# namespaced alias matches the other dependencies, and a misspelled "stb::" name fails when
# CMake configures instead of when the linker runs.
add_library(stb_image INTERFACE)
add_library(stb::image ALIAS stb_image)
target_include_directories(stb_image SYSTEM INTERFACE ${stb_SOURCE_DIR})

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
