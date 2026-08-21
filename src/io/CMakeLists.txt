add_library(glint_io STATIC dataset.cpp)
target_include_directories(glint_io PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(glint_io PUBLIC glint_core stb_image)
