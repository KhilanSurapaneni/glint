add_subdirectory(gpu)
add_subdirectory(viewer)

add_executable(glint main.cpp)
target_link_libraries(glint PRIVATE glint_gpu glint_viewer)
