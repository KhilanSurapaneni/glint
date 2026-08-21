include(GoogleTest)

add_executable(test_gpu_add test_gpu_add.cpp)
target_link_libraries(test_gpu_add PRIVATE glint_gpu GTest::gtest_main)
glint_add_metal_shaders(test_gpu_add ${CMAKE_SOURCE_DIR}/src/shaders/add.metal)

gtest_discover_tests(test_gpu_add)
