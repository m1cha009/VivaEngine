# viva_compile_shaders(<target> SOURCES <file>...)
#
# Compiles GLSL shaders to SPIR-V with glslc whenever <target> is built, and writes each
# result next to the executables as build/<preset>/bin/shaders/<file>.spv (Triangle.vert
# becomes Triangle.vert.spv). glslc picks the shader stage from the file extension:
# .vert, .frag, .comp, ...
#
# Why compile at build time: Vulkan doesn't accept GLSL text at all, only SPIR-V, a compact
# binary format. Compiling during the build turns shader syntax errors into build errors, the
# same way Unity compiles shaders when it imports them, not when the game runs.
#
# Example (from M4 on):
#   viva_compile_shaders(Sandbox SOURCES ${PROJECT_SOURCE_DIR}/shaders/Triangle.vert)
function(viva_compile_shaders target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES")

    # Assumes a single-config generator like Ninja (all our presets use it), where the
    # executables sit directly in CMAKE_RUNTIME_OUTPUT_DIRECTORY.
    set(outputDir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
    set(spirvFiles "")

    foreach(source IN LISTS arg_SOURCES)
        # Relative paths are taken relative to the calling CMakeLists.txt.
        get_filename_component(source "${source}" ABSOLUTE)
        get_filename_component(fileName "${source}" NAME)
        set(spirv "${outputDir}/${fileName}.spv")

        # -g keeps debug info so RenderDoc can show the GLSL source in Debug builds;
        # -O optimizes the SPIR-V in Release.
        add_custom_command(
            OUTPUT "${spirv}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${outputDir}"
            COMMAND Vulkan::glslc --target-env=vulkan1.3
                    "$<IF:$<CONFIG:Debug>,-g,-O>"
                    -o "${spirv}" "${source}"
            DEPENDS "${source}"
            COMMENT "Compiling shader ${fileName}"
            VERBATIM)

        list(APPEND spirvFiles "${spirv}")
    endforeach()

    # The custom target owns the .spv files and <target> depends on it, so building <target>
    # recompiles exactly the shaders whose source changed.
    add_custom_target(${target}Shaders DEPENDS ${spirvFiles} SOURCES ${arg_SOURCES})
    add_dependencies(${target} ${target}Shaders)
endfunction()
