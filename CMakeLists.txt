# VivaEngine root build script.
#
# CMake doesn't compile anything itself. It reads these CMakeLists.txt files and generates a
# build system (Ninja files, in our case), which then runs the compiler and linker. It's a bit
# like Unity generating .csproj files from your asmdefs, except CMake also drives the build.

cmake_minimum_required(VERSION 3.25)

project(VivaEngine
    VERSION 0.1.0
    DESCRIPTION "A small game engine, built to learn how game engines work"
    LANGUAGES C CXX)

# Standard C++ only: -std=c++20 instead of the compiler-specific dialect -std=gnu++20.
set(CMAKE_CXX_EXTENSIONS OFF)

# Collect every executable in build/<preset>/bin/ and every library in build/<preset>/lib/
# instead of scattering them across the build tree. Compiled shaders go into bin/ as well
# (see cmake/Shaders.cmake), so the executable finds them next to itself.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)
include(CompilerWarnings)
include(Shaders)

add_subdirectory(engine)
add_subdirectory(sandbox)
