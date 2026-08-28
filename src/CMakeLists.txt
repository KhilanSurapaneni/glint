add_subdirectory(core)
add_subdirectory(io)
add_subdirectory(gpu)
add_subdirectory(splat)
add_subdirectory(viewer)

add_executable(glint main.cpp)
target_link_libraries(glint PRIVATE glint_gpu glint_viewer glint_io)
glint_depends_on_metallib(glint)  # make sure default.metallib is built before glint is
