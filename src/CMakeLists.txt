add_subdirectory(gpu)

add_executable(glint main.cpp)
target_link_libraries(glint PRIVATE glfw)
