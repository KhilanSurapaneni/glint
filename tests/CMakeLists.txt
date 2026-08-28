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

# SplatSoA owns real GPU buffers, and initialize_from_frames dispatches the real
# unproject_pixel kernel — needs the metallib and, for the real-data test, Replica itself
# (skips gracefully if absent, same as test_dataset.cpp).
add_executable(test_splat_model test_splat_model.cpp)
target_link_libraries(test_splat_model PRIVATE glint_splat glint_gpu glint_io GTest::gtest_main)
target_compile_definitions(test_splat_model PRIVATE GLINT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
glint_depends_on_metallib(test_splat_model)
gtest_discover_tests(test_splat_model)

# Also GPU-free -- covariance.hpp is pure math, verified on the CPU before Phase 2.4 ports the
# same formulas into a real Metal kernel.
add_executable(test_covariance test_covariance.cpp)
target_link_libraries(test_covariance PRIVATE glint_core GTest::gtest_main)
gtest_discover_tests(test_covariance)

add_executable(test_spherical_harmonics test_spherical_harmonics.cpp)
target_link_libraries(test_spherical_harmonics PRIVATE glint_core GTest::gtest_main)
gtest_discover_tests(test_spherical_harmonics)

# The GPU-vs-CPU consistency check for preprocess.metal -- needs both the CPU reference math
# (glint_core) and a real dispatch of the kernel itself (glint_gpu + the metallib).
add_executable(test_preprocess test_preprocess.cpp)
target_link_libraries(test_preprocess PRIVATE glint_core glint_gpu GTest::gtest_main)
glint_depends_on_metallib(test_preprocess)
gtest_discover_tests(test_preprocess)

# Entirely CPU-side (Stage A, Open Decision #3) -- no GPU dependency at all.
add_executable(test_sort test_sort.cpp)
target_link_libraries(test_sort PRIVATE glint_splat GTest::gtest_main)
gtest_discover_tests(test_sort)

# The end-to-end correctness check for the whole forward pass (preprocess -> sort -> raster).
add_executable(test_rasterizer test_rasterizer.cpp)
target_link_libraries(test_rasterizer PRIVATE glint_splat glint_core glint_gpu GTest::gtest_main)
glint_depends_on_metallib(test_rasterizer)
gtest_discover_tests(test_rasterizer)

# Pure CPU math -- no GPU dependency.
add_executable(test_loss test_loss.cpp)
target_link_libraries(test_loss PRIVATE glint_splat GTest::gtest_main)
gtest_discover_tests(test_loss)
