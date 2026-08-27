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

# io/ has no GPU dependency either — same reasoning as test_core_types.
add_executable(test_dataset test_dataset.cpp)
target_link_libraries(test_dataset PRIVATE glint_io GTest::gtest_main)
# Bakes the repo's absolute path into the binary so it can find assets/replica/ regardless of
# what directory the test actually runs from.
target_compile_definitions(test_dataset PRIVATE GLINT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(test_dataset)

# Proves shared_types.h's memory layout actually matches between C++ and MSL — see CLAUDE.md §8.
add_executable(test_layout test_layout.cpp)
target_link_libraries(test_layout PRIVATE glint_gpu GTest::gtest_main)
glint_depends_on_metallib(test_layout)
gtest_discover_tests(test_layout)

add_executable(test_unproject test_unproject.cpp)
target_link_libraries(test_unproject PRIVATE glint_gpu GTest::gtest_main)
glint_depends_on_metallib(test_unproject)
gtest_discover_tests(test_unproject)

# No GPU dependency — this only exercises capture_format.cpp's encode/decode and
# capture_bundle.cpp's file reader, same reasoning as test_dataset.
add_executable(test_capture_bundle test_capture_bundle.cpp)
target_link_libraries(test_capture_bundle PRIVATE glint_io GTest::gtest_main)
gtest_discover_tests(test_capture_bundle)
