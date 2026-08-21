include(FetchContent)

# metal-cpp: Apple's C++ bindings for the Metal API.
FetchContent_Declare(
  metal_cpp
  GIT_REPOSITORY https://github.com/apple/metal-cpp
  GIT_TAG release/metal-cpp_macOS26_iOS26
)
FetchContent_MakeAvailable(metal_cpp)

# metal-cpp has no CMakeLists.txt of its own (just loose headers), so wrap it in an INTERFACE
# target carrying its include path and the Apple frameworks it needs.
add_library(metal_cpp INTERFACE)
target_include_directories(metal_cpp INTERFACE ${metal_cpp_SOURCE_DIR})
target_link_libraries(metal_cpp INTERFACE
  "-framework Foundation"
  "-framework Metal"
  "-framework QuartzCore"
)

# GoogleTest: the testing framework used under tests/.
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest
  GIT_TAG v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # avoids a runtime-library mismatch with our code
FetchContent_MakeAvailable(googletest)

# GLFW: cross-platform window/input handling.
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
