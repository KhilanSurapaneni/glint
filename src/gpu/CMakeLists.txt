add_library(glint_gpu STATIC device.cpp)
target_include_directories(glint_gpu PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(glint_gpu PUBLIC metal_cpp)
