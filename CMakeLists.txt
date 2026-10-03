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

# Put every executable in build/<preset>/bin/. Compiled shaders go there too (see
# cmake/Shaders.cmake), so the executable finds them next to itself.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(cmake/Dependencies.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Shaders.cmake)

add_subdirectory(engine)
add_subdirectory(sandbox)
