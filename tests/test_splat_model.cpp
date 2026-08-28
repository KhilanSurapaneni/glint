#include <gtest/gtest.h>

#include "gpu/device.hpp"
#include "io/dataset.hpp"
#include "splat/model.hpp"

#include <cmath>
#include <filesystem>

TEST(SplatModel, ScaleActivationRoundTrips) {
  for (float scale : {0.001f, 0.1f, 1.0f, 5.0f, 100.0f}) {
    const float log_scale = glint::splat::log_from_scale(scale);
    const float recovered = glint::splat::scale_from_log(log_scale);
    EXPECT_NEAR(recovered, scale, scale * 1e-5f);
  }
}

TEST(SplatModel, OpacityActivationRoundTrips) {
  for (float opacity : {0.001f, 0.1f, 0.5f, 0.9f, 0.999f}) {
    const float logit = glint::splat::logit_from_opacity(opacity);
    const float recovered = glint::splat::opacity_from_logit(logit);
    EXPECT_NEAR(recovered, opacity, 1e-5f);
  }
}

// The whole point of storing these raw — no real log_scale/logit input, however extreme,
// should ever be able to produce a scale/opacity outside its valid closed range. Real-valued
// sigmoid only *approaches* 0/1 asymptotically, but float32 saturates to exactly 0.0f/1.0f
// well before that — e.g. sigmoid(-100) rounds to exactly 0 because exp(100) is so large that
// 1/(1+exp(100)) is smaller than float32 can represent. That's expected, not a bug: an
// opacity of exactly 0 or 1 is a perfectly valid (if extreme) input to alpha compositing
// later — so the bound here is closed, not open.
TEST(SplatModel, ActivationsStayInValidClosedRangeAtExtremeInputs) {
  for (float raw : {-1000.0f, -100.0f, -1.0f, 0.0f, 1.0f, 100.0f, 1000.0f}) {
    EXPECT_GE(glint::splat::scale_from_log(raw), 0.0f);

    const float opacity = glint::splat::opacity_from_logit(raw);
    EXPECT_GE(opacity, 0.0f);
    EXPECT_LE(opacity, 1.0f);
  }
}

// In the moderate range training actually operates in day to day, both activations should be
// strictly interior — this is where gradients are informative. (Worth knowing as a real
// training risk, separate from this test: once opacity saturates all the way to 0 or 1,
// sigmoid's derivative there is ~0, so the optimizer can't move it back — a "dead" splat.)
TEST(SplatModel, ActivationsAreStrictlyInteriorForModerateInputs) {
  for (float raw : {-10.0f, -1.0f, 0.0f, 1.0f, 10.0f}) {
    EXPECT_GT(glint::splat::scale_from_log(raw), 0.0f);

    const float opacity = glint::splat::opacity_from_logit(raw);
    EXPECT_GT(opacity, 0.0f);
    EXPECT_LT(opacity, 1.0f);
  }
}

TEST(SplatModel, QuaternionNormalizes) {
  float x = 2.0f, y = 0.0f, z = 0.0f, w = 0.0f;  // magnitude 2, not a unit quaternion
  glint::splat::normalize_quaternion(x, y, z, w);
  const float magnitude = std::sqrt(x * x + y * y + z * z + w * w);
  EXPECT_NEAR(magnitude, 1.0f, 1e-6f);
}

// Needs a real Metal device — SplatSoA owns real GPU buffers, same reasoning as
// test_gpu_add.cpp.
TEST(SplatModel, SplatSoAHoldsCorrectBufferSizes) {
  glint::gpu::Device device;
  constexpr size_t kSplatCount = 10;
  glint::splat::SplatSoA splats(device.device(), kSplatCount);

  EXPECT_EQ(splats.count, kSplatCount);
  EXPECT_EQ(splats.positions.count(), kSplatCount * 3);
  EXPECT_EQ(splats.rotations.count(), kSplatCount * 4);
  EXPECT_EQ(splats.log_scales.count(), kSplatCount * 3);
  EXPECT_EQ(splats.logit_opacities.count(), kSplatCount);
  EXPECT_EQ(splats.sh_coeffs.count(), kSplatCount * glint::splat::kShCoeffs);
}

// GLINT_SOURCE_DIR is baked in at build time (see tests/CMakeLists.txt) — same reasoning as
// test_dataset.cpp.
TEST(SplatModel, InitializesFromRealReplicaFrames) {
  const std::filesystem::path scene_dir =
      std::filesystem::path(GLINT_SOURCE_DIR) / "assets/replica/Replica/room0";
  if (!std::filesystem::exists(scene_dir)) {
    GTEST_SKIP() << "Replica data not found at " << scene_dir
                 << " — run tools/fetch_replica.sh first.";
  }

  glint::gpu::Device device;
  const glint::io::ReplicaScene scene = glint::io::load_replica_scene(scene_dir, /*max_frames=*/5);

  constexpr size_t kTargetSplatCount = 20000;
  const glint::splat::SplatSoA splats = glint::splat::initialize_from_frames(
      device, scene.frames, scene.camera, kTargetSplatCount);

  // "Roughly" the target — the stride-based subsample won't hit it exactly, but should be in
  // the right ballpark, and never over-shoot the actual number of pixels available.
  EXPECT_GT(splats.count, kTargetSplatCount / 2);
  EXPECT_LT(splats.count, kTargetSplatCount * 2);

  for (size_t i = 0; i < splats.count; ++i) {
    for (int c = 0; c < 3; ++c) {
      EXPECT_TRUE(std::isfinite(splats.positions.data()[i * 3 + c])) << "splat " << i;
      EXPECT_TRUE(std::isfinite(splats.log_scales.data()[i * 3 + c])) << "splat " << i;
    }
    EXPECT_TRUE(std::isfinite(splats.logit_opacities.data()[i])) << "splat " << i;

    // Every splat starts at the same fixed opacity — decode it back and check it lands where
    // initialize_from_frames intended (0.1), not just that it's finite.
    const float opacity = glint::splat::opacity_from_logit(splats.logit_opacities.data()[i]);
    EXPECT_NEAR(opacity, 0.1f, 1e-4f) << "splat " << i;

    // Identity rotation at init.
    EXPECT_FLOAT_EQ(splats.rotations.data()[i * 4 + 0], 0.0f);
    EXPECT_FLOAT_EQ(splats.rotations.data()[i * 4 + 1], 0.0f);
    EXPECT_FLOAT_EQ(splats.rotations.data()[i * 4 + 2], 0.0f);
    EXPECT_FLOAT_EQ(splats.rotations.data()[i * 4 + 3], 1.0f);

    for (int c = 0; c < glint::splat::kShCoeffs; ++c) {
      EXPECT_TRUE(std::isfinite(splats.sh_coeffs.data()[i * glint::splat::kShCoeffs + c]))
          << "splat " << i << " sh " << c;
    }
  }
}
