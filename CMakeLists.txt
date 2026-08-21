cmake_minimum_required(VERSION 3.24)
project(glint CXX)
enable_language(OBJCXX)  # needed to compile src/viewer/metal_layer_bridge.mm

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

# Every executable's output lands directly in build/, so default.metallib (also written to
# build/) is always next to whichever binary needs to load it.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(cmake/Dependencies.cmake)        # fetches metal-cpp, GoogleTest, GLFW
include(cmake/CompileMetalShaders.cmake) # defines the shader-compiling helper functions

# Every .metal file in the project gets listed here, compiled into one shared default.metallib.
glint_compile_metal_shaders(
  ${CMAKE_SOURCE_DIR}/src/shaders/add.metal
  ${CMAKE_SOURCE_DIR}/src/shaders/triangle.metal
)

enable_testing()  # turns on `ctest`

add_subdirectory(src)
add_subdirectory(tests)
