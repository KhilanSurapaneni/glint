cmake_minimum_required(VERSION 3.24)
project(glint CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# All executables land directly in build/, regardless of which subdirectory defined them, so
# default.metallib (also written to build/) is always alongside the binary that loads it.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(cmake/Dependencies.cmake)
include(cmake/CompileMetalShaders.cmake)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
