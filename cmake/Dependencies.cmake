include(FetchContent)

FetchContent_Declare(
  metal_cpp
  GIT_REPOSITORY https://github.com/apple/metal-cpp
  GIT_TAG release/metal-cpp_macOS26_iOS26
)
FetchContent_MakeAvailable(metal_cpp)

# metal-cpp ships as loose headers with no CMakeLists.txt of its own, so wrap it in an
# INTERFACE target exposing its include root and the Apple frameworks it binds against.
add_library(metal_cpp INTERFACE)
target_include_directories(metal_cpp INTERFACE ${metal_cpp_SOURCE_DIR})
target_link_libraries(metal_cpp INTERFACE
  "-framework Foundation"
  "-framework Metal"
  "-framework QuartzCore"
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest
  GIT_TAG v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)
