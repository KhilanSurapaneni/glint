add_library(glint_gpu STATIC device.cpp)
# PUBLIC: anything that links glint_gpu also inherits this include path and metal_cpp.
target_include_directories(glint_gpu PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(glint_gpu PUBLIC metal_cpp)
