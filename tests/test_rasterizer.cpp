#include <gtest/gtest.h>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/LU>

#include "core/covariance.hpp"
#include "core/spherical_harmonics.hpp"
#include "gpu/device.hpp"
#include "splat/model.hpp"
#include "splat/rasterizer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

// Seeds one splat's fields in an already-allocated SplatSoA at index `i`.
void set_splat(glint::splat::SplatSoA& splats, size_t i, const Eigen::Vector3f& position,
                float scale, float opacity, const Eigen::Vector3f& color) {
  splats.positions.data()[i * 3 + 0] = position.x();
  splats.positions.data()[i * 3 + 1] = position.y();
  splats.positions.data()[i * 3 + 2] = position.z();

  splats.rotations.data()[i * 4 + 0] = 0.0f;
  splats.rotations.data()[i * 4 + 1] = 0.0f;
  splats.rotations.data()[i * 4 + 2] = 0.0f;
  splats.rotations.data()[i * 4 + 3] = 1.0f;

  const float log_scale = glint::splat::log_from_scale(scale);
  splats.log_scales.data()[i * 3 + 0] = log_scale;
  splats.log_scales.data()[i * 3 + 1] = log_scale;
  splats.log_scales.data()[i * 3 + 2] = log_scale;

  splats.logit_opacities.data()[i] = glint::splat::logit_from_opacity(opacity);

  float* sh = &splats.sh_coeffs.data()[i * glint::core::kShCoeffs];
  for (int c = 0; c < glint::core::kShCoeffs; ++c) {
    sh[c] = 0.0f;
  }
  sh[0 * glint::core::kShCoeffsPerChannel] = (color.x() - 0.5f) / glint::core::kShC0;
  sh[1 * glint::core::kShCoeffsPerChannel] = (color.y() - 0.5f) / glint::core::kShC0;
  sh[2 * glint::core::kShCoeffsPerChannel] = (color.z() - 0.5f) / glint::core::kShC0;
}

// Independently reproduces the front-to-back compositing formula raster_fwd.metal
// implements, for a manually nearest-first-ordered list of splats — this, plus
// core::covariance_2d (already verified in test_covariance.cpp), is the actual correctness
// check against the real GPU kernel below.
Eigen::Vector3f composite_reference(const std::vector<Eigen::Matrix2f>& covariances_near_to_far,
                                     const std::vector<Eigen::Vector2f>& screen_positions,
                                     const std::vector<float>& opacities,
                                     const std::vector<Eigen::Vector3f>& colors,
                                     const Eigen::Vector2f& pixel_center) {
  Eigen::Vector3f accumulated = Eigen::Vector3f::Zero();
  float transmittance = 1.0f;
  for (size_t i = 0; i < covariances_near_to_far.size(); ++i) {
    const Eigen::Vector2f delta = pixel_center - screen_positions[i];
    const Eigen::Matrix2f inv_cov = covariances_near_to_far[i].inverse();
    const float mahalanobis_sq = delta.transpose() * inv_cov * delta;
    const float alpha = std::min(opacities[i] * std::exp(-0.5f * mahalanobis_sq), 0.99f);
    accumulated += transmittance * alpha * colors[i];
    transmittance *= (1.0f - alpha);
  }
  return accumulated;
}

}  // namespace

// The real correctness check for Phase 4: render two fully-overlapping splats (same screen
// position, different depths) through the actual GPU pipeline (preprocess -> sort -> raster)
// and compare against an independently computed expected value.
TEST(Rasterizer, MatchesIndependentCompositingReference) {
  glint::gpu::Device device;

  constexpr int kWidth = 16, kHeight = 16;  // exactly one 16x16 tile -- no boundary cases
  glint::core::Camera camera;
  camera.width = kWidth;
  camera.height = kHeight;
  camera.fx = 50.0f;
  camera.fy = 50.0f;
  camera.cx = 8.0f;
  camera.cy = 8.0f;

  glint::core::Pose pose;
  pose.camera_to_world = Eigen::Matrix4f::Identity();

  glint::splat::SplatSoA splats(device.device(), 2);
  // Splat 0: farther (z=4), red. Splat 1: nearer (z=2), green -- both centered on-axis, so
  // both project to the same screen position and fully overlap.
  set_splat(splats, 0, Eigen::Vector3f(0.0f, 0.0f, 4.0f), 0.3f, 0.6f,
            Eigen::Vector3f(1.0f, 0.0f, 0.0f));
  set_splat(splats, 1, Eigen::Vector3f(0.0f, 0.0f, 2.0f), 0.3f, 0.5f,
            Eigen::Vector3f(0.0f, 1.0f, 0.0f));

  const glint::splat::RenderedImage image = glint::splat::render(device, splats, camera, pose);

  // --- Independent reference, nearest-first (splat 1, then splat 0) ---
  const Eigen::Matrix3f identity_rotation = Eigen::Matrix3f::Identity();
  const Eigen::Matrix3f sigma_3d = glint::core::covariance_3d(
      Eigen::Quaternionf::Identity(), Eigen::Vector3f(0.3f, 0.3f, 0.3f));

  const Eigen::Vector3f pos_far(0.0f, 0.0f, 4.0f);
  const Eigen::Vector3f pos_near(0.0f, 0.0f, 2.0f);

  const Eigen::Matrix2f cov_far = glint::core::apply_antialiasing_filter(
      glint::core::covariance_2d(sigma_3d, identity_rotation, pos_far, camera.fx, camera.fy));
  const Eigen::Matrix2f cov_near = glint::core::apply_antialiasing_filter(
      glint::core::covariance_2d(sigma_3d, identity_rotation, pos_near, camera.fx, camera.fy));

  const Eigen::Vector2f screen_pos(camera.fx * 0.0f / pos_far.z() + camera.cx,
                                    camera.fy * 0.0f / pos_far.z() + camera.cy);
  const Eigen::Vector2f pixel_center(8.5f, 8.5f);  // center of pixel (8, 8)

  const Eigen::Vector3f expected = composite_reference(
      {cov_near, cov_far}, {screen_pos, screen_pos}, {0.5f, 0.6f},
      {Eigen::Vector3f(0.0f, 1.0f, 0.0f), Eigen::Vector3f(1.0f, 0.0f, 0.0f)}, pixel_center);

  const size_t pixel_index = static_cast<size_t>(8 * kWidth + 8) * 3;
  EXPECT_NEAR(image.rgb[pixel_index + 0], expected.x(), 1e-3f);
  EXPECT_NEAR(image.rgb[pixel_index + 1], expected.y(), 1e-3f);
  EXPECT_NEAR(image.rgb[pixel_index + 2], expected.z(), 1e-3f);
}

// A pixel far from a splat, relative to its scale, should stay pure background: (0, 0, 0) —
// nothing contributes meaningfully there.
TEST(Rasterizer, PixelsFarFromAnySplatStayBlack) {
  glint::gpu::Device device;

  constexpr int kWidth = 16, kHeight = 16;
  glint::core::Camera camera;
  camera.width = kWidth;
  camera.height = kHeight;
  camera.fx = 50.0f;
  camera.fy = 50.0f;
  camera.cx = 8.0f;
  camera.cy = 8.0f;

  glint::core::Pose pose;
  pose.camera_to_world = Eigen::Matrix4f::Identity();

  // Deliberately much smaller than the 0.3 scale used above — checked numerically
  // beforehand that 0.3 would actually still reach this corner pixel with a small but
  // non-negligible alpha (~0.018, above the kernel's 1/255 cutoff), so this test needs a
  // splat whose footprint is genuinely tiny relative to the image, not just "smaller."
  glint::splat::SplatSoA splats(device.device(), 1);
  set_splat(splats, 0, Eigen::Vector3f(0.0f, 0.0f, 4.0f), 0.05f, 0.9f,
            Eigen::Vector3f(1.0f, 1.0f, 1.0f));

  const glint::splat::RenderedImage image = glint::splat::render(device, splats, camera, pose);

  const size_t corner_index = static_cast<size_t>(0 * kWidth + 0) * 3;  // pixel (0, 0)
  EXPECT_NEAR(image.rgb[corner_index + 0], 0.0f, 1e-4f);
  EXPECT_NEAR(image.rgb[corner_index + 1], 0.0f, 1e-4f);
  EXPECT_NEAR(image.rgb[corner_index + 2], 0.0f, 1e-4f);
}
