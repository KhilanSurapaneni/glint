include(GoogleTest)

add_executable(test_gpu_add test_gpu_add.cpp)
target_link_libraries(test_gpu_add PRIVATE glint_gpu GTest::gtest_main)
glint_depends_on_metallib(test_gpu_add)

# Registers each TEST(...) found in test_gpu_add as its own ctest entry.
gtest_discover_tests(test_gpu_add)

# No glint_depends_on_metallib here — core/ has no GPU dependency at all, and this test proves it.
add_executable(test_core_types test_core_types.cpp)
target_link_libraries(test_core_types PRIVATE glint_core GTest::gtest_main)
gtest_discover_tests(test_core_types)
