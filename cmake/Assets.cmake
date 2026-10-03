# viva_copy_assets(<target> <file>...)
#
# Copies game assets (textures, models...) from the assets/ folder to the same place next to the
# executables, build/<preset>/bin/assets/, whenever <target> is built. Files are named relative
# to assets/, for example textures/crate.png. Like shaders, the game finds them relative to the
# executable at runtime (see GetAssetPath in engine/src/Platform/FileSystem.h), and install()
# puts them in the packaged build too.
#
# Call it once per target, listing all of that target's assets.
function(viva_copy_assets target)
    set(copies "")
    foreach(file IN LISTS ARGN)
        set(source "${PROJECT_SOURCE_DIR}/assets/${file}")
        set(destination "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/${file}")
        # copy_if_different leaves the file's timestamp alone when nothing changed, so builds
        # that depend on it don't redo work.
        add_custom_command(
            OUTPUT "${destination}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${source}" "${destination}"
            DEPENDS "${source}"
            COMMENT "Copying asset ${file}"
            VERBATIM)
        list(APPEND copies "${destination}")

        get_filename_component(folder "${file}" DIRECTORY)
        install(FILES "${source}" DESTINATION "assets/${folder}")
    endforeach()

    if(copies)
        add_custom_target(${target}Assets DEPENDS ${copies})
        add_dependencies(${target} ${target}Assets)
    endif()
endfunction()
