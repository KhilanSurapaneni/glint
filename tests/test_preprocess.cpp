#include <gtest/gtest.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "core/covariance.hpp"
#include "core/spherical_harmonics.hpp"
#include "gpu/buffer.hpp"
#include "gpu/device.hpp"
#include "gpu/kernel.hpp"
#include "shaders/shared_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Verifies preprocess.metal's GPU kernel produces the same results as the CPU reference math
// in core/covariance.hpp and core/spherical_harmonics.hpp — the real correctness check for
// Phase 2's kernel port, not just "it compiles and runs." Uses an identity view matrix and a
// camera at the world origin so camera-space position equals world position directly, keeping
// the hand-computed expected values simple without giving up real kernel coverage — the
// kernel itself doesn't know or care that the view happens to be identity.
TEST(Preprocess, MatchesCpuReferenceForOneSplat) {
  glint::gpu::Device device;

  // --- One splat's parameters ---
  const Eigen::Vector3f world_position(1.0f, 2.0f, 5.0f);
  const Eigen::Quaternionf rotation(
      Eigen::AngleAxisf(0.4f, Eigen::Vector3f(0.0f, 1.0f, 0.0f)));
  const Eigen::Vector3f scale(0.1f, 0.2f, 0.05f);

  float sh[glint::core::kShCoeffs] = {};
  sh[0 * glint::core::kShCoeffsPerChannel] = 1.2f;      // R, degree 0
  sh[1 * glint::core::kShCoeffsPerChannel] = -0.3f;     // G, degree 0
  sh[2 * glint::core::kShCoeffsPerChannel] = 0.7f;      // B, degree 0
  sh[0 * glint::core::kShCoeffsPerChannel + 2] = 0.5f;  // R, degree 1 — exercises past degree 0

  // --- Camera: identity view, at the world origin. ---
  const Eigen::Vector3f camera_position(0.0f, 0.0f, 0.0f);
  constexpr float kFx = 500.0f, kFy = 480.0f, kCx = 320.0f, kCy = 240.0f;
  constexpr uint32_t kWidth = 640, kHeight = 480;

  // --- CPU reference ---
  const Eigen::Matrix3f sigma_3d = glint::core::covariance_3d(rotation, scale);
  const Eigen::Matrix3f view_rotation = Eigen::Matrix3f::Identity();
  const Eigen::Matrix2f sigma_2d_raw =
      glint::core::covariance_2d(sigma_3d, view_rotation, world_position, kFx, kFy);
  const Eigen::Matrix2f sigma_2d = glint::core::apply_antialiasing_filter(sigma_2d_raw);
  const float expected_radius = glint::core::screen_space_radius(sigma_2d);

  const float expected_screen_x = kFx * world_position.x() / world_position.z() + kCx;
  const float expected_screen_y = kFy * world_position.y() / world_position.z() + kCy;

  const Eigen::Vector3f view_direction = (world_position - camera_position).normalized();
  const Eigen::Vector3f expected_color = glint::core::evaluate_sh_color(sh, view_direction);

  const float logit_opacity = -0.8f;  // arbitrary raw value; sigmoid(-0.8) is the expected decode
  const float expected_opacity = 1.0f / (1.0f + std::exp(-logit_opacity));

  // --- GPU: same inputs, through the real kernel ---
  PreprocessParams params{};
  params.view_matrix =
      simd::float4x4(simd::float4{1.0f, 0.0f, 0.0f, 0.0f}, simd::float4{0.0f, 1.0f, 0.0f, 0.0f},
                      simd::float4{0.0f, 0.0f, 1.0f, 0.0f}, simd::float4{0.0f, 0.0f, 0.0f, 1.0f});
  params.camera_position_x = camera_position.x();
  params.camera_position_y = camera_position.y();
  params.camera_position_z = camera_position.z();
  params.fx = kFx;
  params.fy = kFy;
  params.cx = kCx;
  params.cy = kCy;
  params.image_width = kWidth;
  params.image_height = kHeight;

  glint::gpu::Buffer<float> positions_buffer(device.device(), 3);
  positions_buffer.data()[0] = world_position.x();
  positions_buffer.data()[1] = world_position.y();
  positions_buffer.data()[2] = world_position.z();

  glint::gpu::Buffer<float> rotations_buffer(device.device(), 4);
  rotations_buffer.data()[0] = rotation.x();
  rotations_buffer.data()[1] = rotation.y();
  rotations_buffer.data()[2] = rotation.z();
  rotations_buffer.data()[3] = rotation.w();

  glint::gpu::Buffer<float> log_scales_buffer(device.device(), 3);
  log_scales_buffer.data()[0] = std::log(scale.x());
  log_scales_buffer.data()[1] = std::log(scale.y());
  log_scales_buffer.data()[2] = std::log(scale.z());

  glint::gpu::Buffer<float> sh_buffer(device.device(), glint::core::kShCoeffs);
  std::copy(std::begin(sh), std::end(sh), sh_buffer.data());

  glint::gpu::Buffer<float> logit_opacities_buffer(device.device(), 1);
  logit_opacities_buffer.data()[0] = logit_opacity;

  glint::gpu::Buffer<PreprocessParams> params_buffer(device.device(), 1);
  params_buffer.data()[0] = params;

  glint::gpu::Buffer<float> out_screen_positions(device.device(), 2);
  glint::gpu::Buffer<float> out_covariances(device.device(), 3);
  glint::gpu::Buffer<float> out_colors(device.device(), 3);
  glint::gpu::Buffer<float> out_opacities(device.device(), 1);
  glint::gpu::Buffer<float> out_radii(device.device(), 1);
  glint::gpu::Buffer<uint32_t> out_tile_touch_counts(device.device(), 1);
  glint::gpu::Buffer<float> out_depths(device.device(), 1);

  glint::gpu::Kernel kernel(device.device(), device.library(), "preprocess_splats");
  glint::gpu::dispatch_and_wait(
      device.queue(), kernel,
      {positions_buffer.handle(), rotations_buffer.handle(), log_scales_buffer.handle(),
       sh_buffer.handle(), logit_opacities_buffer.handle(), params_buffer.handle(),
       out_screen_positions.handle(), out_covariances.handle(), out_colors.handle(),
       out_opacities.handle(), out_radii.handle(), out_tile_touch_counts.handle(),
       out_depths.handle()},
      1);

  // --- Compare ---
  EXPECT_NEAR(out_screen_positions.data()[0], expected_screen_x, 1e-2f);
  EXPECT_NEAR(out_screen_positions.data()[1], expected_screen_y, 1e-2f);
  // Identity view, camera at the origin -> camera-space depth is just world_position.z().
  EXPECT_NEAR(out_depths.data()[0], world_position.z(), 1e-4f);

  EXPECT_NEAR(out_covariances.data()[0], sigma_2d(0, 0), 1e-3f);
  EXPECT_NEAR(out_covariances.data()[1], sigma_2d(0, 1), 1e-3f);
  EXPECT_NEAR(out_covariances.data()[2], sigma_2d(1, 1), 1e-3f);

  EXPECT_NEAR(out_colors.data()[0], expected_color.x(), 1e-3f);
  EXPECT_NEAR(out_colors.data()[1], expected_color.y(), 1e-3f);
  EXPECT_NEAR(out_colors.data()[2], expected_color.z(), 1e-3f);

  EXPECT_NEAR(out_opacities.data()[0], expected_opacity, 1e-5f);

  EXPECT_NEAR(out_radii.data()[0], expected_radius, 1e-2f);

  // Well within frame, so it must touch at least one tile.
  EXPECT_GT(out_tile_touch_counts.data()[0], 0u);
}

// A splat placed behind the camera must be culled — zero tiles touched, the sort/raster
// stages' signal to skip it entirely.
TEST(Preprocess, CullsSplatsBehindTheCamera) {
  glint::gpu::Device device;

  PreprocessParams params{};
  params.view_matrix =
      simd::float4x4(simd::float4{1.0f, 0.0f, 0.0f, 0.0f}, simd::float4{0.0f, 1.0f, 0.0f, 0.0f},
                      simd::float4{0.0f, 0.0f, 1.0f, 0.0f}, simd::float4{0.0f, 0.0f, 0.0f, 1.0f});
  params.fx = 500.0f;
  params.fy = 480.0f;
  params.cx = 320.0f;
  params.cy = 240.0f;
  params.image_width = 640;
  params.image_height = 480;

  glint::gpu::Buffer<float> positions_buffer(device.device(), 3);
  positions_buffer.data()[0] = 0.0f;
  positions_buffer.data()[1] = 0.0f;
  positions_buffer.data()[2] = -5.0f;  // behind the camera (identity view looks down +z)

  glint::gpu::Buffer<float> rotations_buffer(device.device(), 4);
  rotations_buffer.data()[0] = 0.0f;
  rotations_buffer.data()[1] = 0.0f;
  rotations_buffer.data()[2] = 0.0f;
  rotations_buffer.data()[3] = 1.0f;

  glint::gpu::Buffer<float> log_scales_buffer(device.device(), 3);
  log_scales_buffer.data()[0] = std::log(0.1f);
  log_scales_buffer.data()[1] = std::log(0.1f);
  log_scales_buffer.data()[2] = std::log(0.1f);

  glint::gpu::Buffer<float> sh_buffer(device.device(), glint::core::kShCoeffs);
  std::fill(sh_buffer.data(), sh_buffer.data() + glint::core::kShCoeffs, 0.0f);

  glint::gpu::Buffer<float> logit_opacities_buffer(device.device(), 1);
  logit_opacities_buffer.data()[0] = 0.0f;

  glint::gpu::Buffer<PreprocessParams> params_buffer(device.device(), 1);
  params_buffer.data()[0] = params;

  glint::gpu::Buffer<float> out_screen_positions(device.device(), 2);
  glint::gpu::Buffer<float> out_covariances(device.device(), 3);
  glint::gpu::Buffer<float> out_colors(device.device(), 3);
  glint::gpu::Buffer<float> out_opacities(device.device(), 1);
  glint::gpu::Buffer<float> out_radii(device.device(), 1);
  glint::gpu::Buffer<uint32_t> out_tile_touch_counts(device.device(), 1);
  out_tile_touch_counts.data()[0] = 999;  // sentinel, so we can tell the kernel actually ran
  glint::gpu::Buffer<float> out_depths(device.device(), 1);

  glint::gpu::Kernel kernel(device.device(), device.library(), "preprocess_splats");
  glint::gpu::dispatch_and_wait(
      device.queue(), kernel,
      {positions_buffer.handle(), rotations_buffer.handle(), log_scales_buffer.handle(),
       sh_buffer.handle(), logit_opacities_buffer.handle(), params_buffer.handle(),
       out_screen_positions.handle(), out_covariances.handle(), out_colors.handle(),
       out_opacities.handle(), out_radii.handle(), out_tile_touch_counts.handle(),
       out_depths.handle()},
      1);

  EXPECT_EQ(out_tile_touch_counts.data()[0], 0u);
}
