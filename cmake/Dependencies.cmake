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
