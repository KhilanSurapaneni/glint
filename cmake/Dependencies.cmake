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

# Dear ImGui: immediate-mode GUI toolkit for on-screen controls/overlays.
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui
  GIT_TAG v1.92.9
)
FetchContent_MakeAvailable(imgui)

# Like metal-cpp, ImGui ships as loose source files with no CMakeLists.txt of its own — build
# its core plus the GLFW and Metal backends ourselves.
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp  # ships ImGui::ShowDemoWindow(), which we use to verify this works
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw "-framework Metal" "-framework QuartzCore")
set_source_files_properties(${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm
  PROPERTIES COMPILE_FLAGS "-fobjc-arc")

# Eigen: CPU linear algebra. Its own CMakeLists.txt pulls in a large test/doc suite we don't
# want polluting our build, so we fetch just the source without running it: SOURCE_SUBDIR
# points at a folder that doesn't exist, so FetchContent_MakeAvailable still populates
# eigen_SOURCE_DIR but finds no CMakeLists.txt there to add_subdirectory. We then wrap the
# real source in our own minimal INTERFACE target — same pattern as metal-cpp.
FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen
  GIT_TAG 3.4.1
  SOURCE_SUBDIR do-not-build-eigens-own-cmake-project
)
FetchContent_MakeAvailable(eigen)

# stb_image: single-header image decoding (JPEG/PNG). No release tags exist for this repo, so
# we pin to a specific commit hash instead of a tag — same reproducibility goal either way.
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb
  GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
)
FetchContent_MakeAvailable(stb)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

add_library(eigen INTERFACE)
target_include_directories(eigen INTERFACE ${eigen_SOURCE_DIR})
