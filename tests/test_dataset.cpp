#include <gtest/gtest.h>

#include <Eigen/Core>

#include "io/dataset.hpp"

#include <filesystem>

// GLINT_SOURCE_DIR is baked in at build time (see tests/CMakeLists.txt) so this test can find
// assets/ regardless of what directory it's actually run from — same reasoning as device.cpp
// resolving default.metallib relative to the executable instead of trusting the working
// directory.
TEST(Dataset, LoadsRealReplicaScene) {
  const std::filesystem::path scene_dir =
      std::filesystem::path(GLINT_SOURCE_DIR) / "assets/replica/Replica/room0";

  if (!std::filesystem::exists(scene_dir)) {
    GTEST_SKIP() << "Replica data not found at " << scene_dir
                 << " — run tools/fetch_replica.sh first.";
  }

  // A handful of frames is enough to prove the loader parses everything correctly — the full
  // 2000-frame scale check lives in the DISABLED_ test below, since loading all of them takes
  // ~95s and isn't worth paying on every ordinary ctest run.
  const glint::io::ReplicaScene scene = glint::io::load_replica_scene(scene_dir, /*max_frames=*/5);

  ASSERT_EQ(scene.frames.size(), 5u);

  EXPECT_EQ(scene.camera.width, 1200);
  EXPECT_EQ(scene.camera.height, 680);
  EXPECT_FLOAT_EQ(scene.camera.fx, 600.0f);
  EXPECT_FLOAT_EQ(scene.camera.fy, 600.0f);
  EXPECT_FLOAT_EQ(scene.camera.cx, 599.5f);
  EXPECT_FLOAT_EQ(scene.camera.cy, 339.5f);

  const glint::core::Frame& first = scene.frames[0];

  // allFinite() catches a parsing bug that produced NaN/Inf; isApprox(Identity()) catches
  // one that silently left the pose untouched instead of actually reading traj.txt.
  EXPECT_TRUE(first.pose.camera_to_world.allFinite());
  EXPECT_FALSE(first.pose.camera_to_world.isApprox(Eigen::Matrix4f::Identity()));

  EXPECT_EQ(first.rgb.size(), static_cast<size_t>(1200 * 680 * 3));
  EXPECT_EQ(first.depth.size(), static_cast<size_t>(1200 * 680));
}

// Skipped by default (GoogleTest's DISABLED_ convention) — full 2000-frame load, ~95s. Run
// explicitly with `./build/test_dataset --gtest_also_run_disabled_tests` when you actually
// want to verify the whole loader end-to-end (e.g. before closing out a milestone).
TEST(Dataset, DISABLED_LoadsFullReplicaScene) {
  const std::filesystem::path scene_dir =
      std::filesystem::path(GLINT_SOURCE_DIR) / "assets/replica/Replica/room0";

  if (!std::filesystem::exists(scene_dir)) {
    GTEST_SKIP() << "Replica data not found at " << scene_dir
                 << " — run tools/fetch_replica.sh first.";
  }

  const glint::io::ReplicaScene scene = glint::io::load_replica_scene(scene_dir);
  ASSERT_EQ(scene.frames.size(), 2000u);
}
