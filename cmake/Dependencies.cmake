# Third-party dependencies.
#
# FetchContent downloads each library at configure time into build/<preset>/_deps/ and builds
# it as part of our project, a bit like Unity's Package Manager pulling a package from a git URL
# pinned to one version. Nothing gets installed system-wide.
#
# SYSTEM marks a library's headers as "system" headers, so the compiler doesn't report
# warnings from inside them. Our /W4 and -Wall output then only covers our own code.

include(FetchContent)

# --- SDL3: window, input and (later) audio --------------------------------------------------
# Built as a static library, so it's linked straight into the executable and there's no
# SDL3.dll / libSDL3.dylib to ship next to it. The options must be set before
# FetchContent_MakeAvailable runs SDL's own CMakeLists.txt.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE
    SYSTEM)

# --- GLM: vector and matrix math ------------------------------------------------------------
# GLM is all templates, so it lives entirely in headers. GLM_BUILD_LIBRARY OFF stops it from
# also compiling an optional glm.lib we don't need. glm::glm is then just "these headers".
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    GIT_SHALLOW TRUE
    SYSTEM)

FetchContent_MakeAvailable(SDL3 glm)

# --- Vulkan: headers and loader from the LunarG Vulkan SDK ----------------------------------
# Not fetched: the SDK is installed on the machine, because it also provides the validation
# layers and the glslc shader compiler. FindVulkan locates it through the VULKAN_SDK environment
# variable (Windows, or macOS after sourcing setup-env.sh) or the system-wide install on macOS.
find_package(Vulkan REQUIRED COMPONENTS glslc)
