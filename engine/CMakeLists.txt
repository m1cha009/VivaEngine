# The engine: a static library that games link against.
#
# A static library (.lib on Windows, .a on macOS) is a bundle of compiled code that the linker
# copies into the final executable. The closest C# equivalent is a class library DLL that your
# game references, except that nothing is loaded at runtime: the code ends up inside the .exe.

add_library(VivaEngine STATIC
    src/Core/Application.cpp
    src/Core/Assert.cpp
    src/Core/Log.cpp
    src/Core/Time.cpp
    src/Core/Version.cpp
    src/Platform/Debugger.cpp
    src/Platform/FileSystem.cpp
    src/Platform/Input.cpp
    src/Platform/SDLVersion.cpp
    src/Platform/Window.cpp
    src/Renderer/Buffer.cpp
    src/Renderer/FrameResources.cpp
    src/Renderer/Mesh.cpp
    src/Renderer/Pipeline.cpp
    src/Renderer/Renderer.cpp
    src/Renderer/Swapchain.cpp
    src/Renderer/VmaImplementation.cpp
    src/Renderer/VulkanCheck.cpp
    src/Renderer/VulkanContext.cpp
    src/Renderer/VulkanVersion.cpp)

# include/ is the public API that games see. src/ is private to the engine.
target_include_directories(VivaEngine
    PUBLIC include
    PRIVATE src)

target_compile_features(VivaEngine PUBLIC cxx_std_20)

# Compile definitions are -D flags for the compiler, much like Unity's Scripting Define Symbols.
# PUBLIC ones also apply to every target that links the engine; PRIVATE ones only to the
# engine's own .cpp files.
target_compile_definitions(VivaEngine
    PUBLIC
        # Defined in Debug builds only. PUBLIC because the VIVA_ASSERT macro and Log, which are
        # expanded inside game code, check it.
        $<$<CONFIG:Debug>:VIVA_DEBUG>
    PRIVATE
        # The single source of truth for the version is project(VERSION ...) in the root
        # CMakeLists.txt.
        VIVA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        VIVA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        VIVA_VERSION_PATCH=${PROJECT_VERSION_PATCH})

if(MSVC)
    # /Zc:preprocessor switches MSVC to its standard-conforming preprocessor. VIVA_ASSERT relies
    # on C++20's __VA_OPT__, and the macro is expanded in whichever file uses it, so this is
    # PUBLIC. /utf-8 reads source files as UTF-8 and stores strings as UTF-8, like Clang does.
    target_compile_options(VivaEngine PUBLIC /Zc:preprocessor /utf-8)
endif()

# PUBLIC: games use GLM math types through the engine's API, so they get GLM's headers (and its
# settings, see cmake/Dependencies.cmake) too.
# PRIVATE: SDL3, VMA and Vulkan are implementation details. Their headers aren't visible to games
# at all, so the sandbox physically can't include them. (Inside the engine, "SDL only in
# Platform/, Vulkan only in Renderer/" is a convention we follow, not something the build
# enforces.) SDL3, GLM and VMA are listed before Vulkan so their headers win if the Vulkan SDK was
# installed with its optional SDL/GLM/VMA copies.
target_link_libraries(VivaEngine
    PUBLIC glm::glm
    PRIVATE SDL3::SDL3-static GPUOpen::VulkanMemoryAllocator Vulkan::Vulkan)

viva_set_warnings(VivaEngine)

# The renderer's shaders, compiled to SPIR-V into bin/shaders/ whenever the engine is built (see
# cmake/Shaders.cmake). The engine loads them from there at runtime.
viva_compile_shaders(VivaEngine
    ${PROJECT_SOURCE_DIR}/shaders/VertexColor.vert
    ${PROJECT_SOURCE_DIR}/shaders/VertexColor.frag)
