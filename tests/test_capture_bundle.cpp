#include <gtest/gtest.h>

#include <Eigen/Core>

#include "io/capture_bundle.hpp"
#include "io/capture_format.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {

// A tiny, hand-built scene — small enough that this test doesn't depend on Replica data
// existing on disk, unlike test_dataset.cpp. Deliberately non-uniform values (not all zeros)
// so a bug that zeroes a field instead of copying it would actually be caught.
glint::core::Frame make_test_frame(int width, int height, uint8_t rgb_seed) {
  glint::core::Frame frame;
  frame.pose.camera_to_world = Eigen::Matrix4f::Identity();
  frame.pose.camera_to_world(0, 3) = 1.5f;  // some translation, so it's not just identity
  frame.pose.camera_to_world(1, 3) = -2.25f;

  frame.rgb.resize(static_cast<size_t>(width) * height * 3);
  for (size_t i = 0; i < frame.rgb.size(); ++i) {
    frame.rgb[i] = static_cast<uint8_t>((i + rgb_seed) % 256);
  }

  frame.depth.resize(static_cast<size_t>(width) * height);
  for (size_t i = 0; i < frame.depth.size(); ++i) {
    frame.depth[i] = 0.5f + static_cast<float>(i % 100) * 0.01f;  // plausible meters
  }

  return frame;
}

}  // namespace

// Writes a small synthetic session with capture_format.cpp's encoder, reads it back with
// capture_bundle.cpp's file reader, and checks the round trip preserves everything —
// pose/depth exactly, RGB within JPEG's lossy tolerance. This is the whole reader's
// correctness proof available before any real capture data exists: nobody's run the iOS app
// on real hardware yet, so this is what "the reader works" means for now.
TEST(CaptureBundle, RoundTripsThroughFile) {
  constexpr int kWidth = 16;
  constexpr int kHeight = 12;

  glint::core::Camera camera;
  camera.width = kWidth;
  camera.height = kHeight;
  camera.fx = 600.0f;
  camera.fy = 601.5f;
  camera.cx = 8.0f;
  camera.cy = 6.0f;

  const std::vector<glint::core::Frame> original_frames = {
      make_test_frame(kWidth, kHeight, /*rgb_seed=*/0),
      make_test_frame(kWidth, kHeight, /*rgb_seed=*/77),
  };

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "glint_test_capture_bundle.glcb";

  {
    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    const glint::io::ByteWriter write_exact = [&out](const void* src, size_t size) {
      out.write(static_cast<const char*>(src), static_cast<std::streamsize>(size));
    };
    glint::io::write_session_header(write_exact, camera);
    for (const glint::core::Frame& frame : original_frames) {
      glint::io::write_frame_record(write_exact, frame, kWidth, kHeight);
    }
  }

  const glint::io::CaptureBundle bundle = glint::io::load_capture_bundle(path);
  std::filesystem::remove(path);

  EXPECT_EQ(bundle.camera.width, kWidth);
  EXPECT_EQ(bundle.camera.height, kHeight);
  EXPECT_FLOAT_EQ(bundle.camera.fx, camera.fx);
  EXPECT_FLOAT_EQ(bundle.camera.fy, camera.fy);
  EXPECT_FLOAT_EQ(bundle.camera.cx, camera.cx);
  EXPECT_FLOAT_EQ(bundle.camera.cy, camera.cy);

  ASSERT_EQ(bundle.frames.size(), original_frames.size());

  for (size_t i = 0; i < bundle.frames.size(); ++i) {
    const glint::core::Frame& expected = original_frames[i];
    const glint::core::Frame& actual = bundle.frames[i];

    EXPECT_TRUE(actual.pose.camera_to_world.isApprox(expected.pose.camera_to_world, 1e-5f))
        << "frame " << i;

    ASSERT_EQ(actual.depth.size(), expected.depth.size()) << "frame " << i;
    for (size_t p = 0; p < actual.depth.size(); ++p) {
      EXPECT_NEAR(actual.depth[p], expected.depth[p], 1e-5f) << "frame " << i << " pixel " << p;
    }

    // RGB goes through lossy JPEG, so exact equality isn't the right bar — a generous
    // per-channel tolerance still catches a real bug (e.g. channel order swapped, or the
    // whole frame just wrong) without false-failing on ordinary compression artifacts.
    ASSERT_EQ(actual.rgb.size(), expected.rgb.size()) << "frame " << i;
    size_t total_abs_diff = 0;
    for (size_t p = 0; p < actual.rgb.size(); ++p) {
      total_abs_diff += static_cast<size_t>(
          std::abs(static_cast<int>(actual.rgb[p]) - static_cast<int>(expected.rgb[p])));
    }
    const double mean_abs_diff = static_cast<double>(total_abs_diff) / actual.rgb.size();
    EXPECT_LT(mean_abs_diff, 5.0) << "frame " << i << " mean abs RGB diff too high after JPEG";
  }
}

// A file that isn't a .glcb at all (or is truncated before a full header arrives) must fail
// loudly, not silently produce a bogus empty/garbage camera.
TEST(CaptureBundle, ThrowsOnBadMagic) {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "glint_test_capture_bundle_bad.glcb";
  {
    std::ofstream out(path, std::ios::binary);
    out.write("NOPE", 4);
  }

  EXPECT_THROW(glint::io::load_capture_bundle(path), std::runtime_error);
  std::filesystem::remove(path);
}
