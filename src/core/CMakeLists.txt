add_library(glint_core INTERFACE)
target_include_directories(glint_core INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(glint_core INTERFACE eigen)
