cmake_minimum_required(VERSION 3.24)
project(glint CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/Dependencies.cmake)

# Phase 2 smoke test only — superseded by src/gpu/device.hpp/.cpp in Phase 4.
add_executable(metal_smoke_test src/gpu/metal_smoke_test.cpp)
target_link_libraries(metal_smoke_test PRIVATE metal_cpp)
