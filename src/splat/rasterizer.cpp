#include "splat/rasterizer.hpp"

#include <Eigen/LU>  // Matrix4f::inverse() for view_matrix

#include "gpu/kernel.hpp"
#include "shaders/shared_types.h"
#include "splat/sort.hpp"

#include <algorithm>
#include <cstdint>

namespace glint::splat {

namespace {

PreprocessParams build_preprocess_params(const core::Camera& camera, const core::Pose& pose) {
  // world-to-camera is the inverse of the stored camera-to-world pose — same relationship
  // dataset.cpp's poses and M1's unprojection already rely on, just run the other direction.
  const Eigen::Matrix4f view_matrix = pose.camera_to_world.inverse();

  PreprocessParams params{};
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      params.view_matrix.columns[col][row] = view_matrix(row, col);
    }
  }
  // The camera's own world-space position is just camera-to-world's translation column —
  // cheaper to read directly than to re-derive it from the inverted view_matrix.
  params.camera_position_x = pose.camera_to_world(0, 3);
  params.camera_position_y = pose.camera_to_world(1, 3);
  params.camera_position_z = pose.camera_to_world(2, 3);
  params.fx = camera.fx;
  params.fy = camera.fy;
  params.cx = camera.cx;
  params.cy = camera.cy;
  params.image_width = static_cast<uint32_t>(camera.width);
  params.image_height = static_cast<uint32_t>(camera.height);
  return params;
}

}  // namespace

RenderedImage render(gpu::Device& device, const SplatSoA& splats, const core::Camera& camera,
                      const core::Pose& pose) {
  const size_t splat_count = splats.count;
  const size_t pixel_count = static_cast<size_t>(camera.width) * camera.height;
  const auto image_width = static_cast<uint32_t>(camera.width);
  const auto image_height = static_cast<uint32_t>(camera.height);

  // --- Phase 2: preprocess ---
  gpu::Buffer<PreprocessParams> params_buffer(device.device(), 1);
  params_buffer.data()[0] = build_preprocess_params(camera, pose);

  gpu::Buffer<float> screen_positions(device.device(), splat_count * 2);
  gpu::Buffer<float> covariances(device.device(), splat_count * 3);
  gpu::Buffer<float> colors(device.device(), splat_count * 3);
  gpu::Buffer<float> opacities(device.device(), splat_count);
  gpu::Buffer<float> radii(device.device(), splat_count);
  gpu::Buffer<uint32_t> tile_touch_counts(device.device(), splat_count);
  gpu::Buffer<float> depths(device.device(), splat_count);

  gpu::Kernel preprocess_kernel(device.device(), device.library(), "preprocess_splats");
  gpu::dispatch_and_wait(
      device.queue(), preprocess_kernel,
      {splats.positions.handle(), splats.rotations.handle(), splats.log_scales.handle(),
       splats.sh_coeffs.handle(), splats.logit_opacities.handle(), params_buffer.handle(),
       screen_positions.handle(), covariances.handle(), colors.handle(), opacities.handle(),
       radii.handle(), tile_touch_counts.handle(), depths.handle()},
      splat_count);

  // --- Phase 3: sort (CPU, Stage A — Open Decision #3) ---
  const SortResult sorted =
      sort_splats_by_tile(screen_positions.data(), depths.data(), radii.data(),
                           tile_touch_counts.data(), splat_count, image_width, image_height);

  const uint32_t tiles_x = (image_width + GLINT_TILE_SIZE - 1) / GLINT_TILE_SIZE;
  const uint32_t tiles_y = (image_height + GLINT_TILE_SIZE - 1) / GLINT_TILE_SIZE;

  // The sort's CPU-only results have to become GPU buffers for the raster kernel to read.
  // max(..., 1): a zero-sized Metal buffer is worth avoiding even in the (valid) case where
  // nothing is visible at all.
  gpu::Buffer<uint32_t> sorted_splat_indices(
      device.device(), std::max<size_t>(sorted.sorted_keys.size(), 1));
  for (size_t i = 0; i < sorted.sorted_keys.size(); ++i) {
    sorted_splat_indices.data()[i] = sorted.sorted_keys[i].splat_index;
  }

  gpu::Buffer<uint32_t> tile_range_starts(device.device(), sorted.tile_ranges.size());
  gpu::Buffer<uint32_t> tile_range_ends(device.device(), sorted.tile_ranges.size());
  for (size_t i = 0; i < sorted.tile_ranges.size(); ++i) {
    tile_range_starts.data()[i] = sorted.tile_ranges[i].start;
    tile_range_ends.data()[i] = sorted.tile_ranges[i].end;
  }

  gpu::Buffer<uint32_t> image_width_buffer(device.device(), 1);
  image_width_buffer.data()[0] = image_width;
  gpu::Buffer<uint32_t> image_height_buffer(device.device(), 1);
  image_height_buffer.data()[0] = image_height;
  gpu::Buffer<uint32_t> tiles_x_buffer(device.device(), 1);
  tiles_x_buffer.data()[0] = tiles_x;

  // --- Phase 4: raster ---
  gpu::Buffer<float> out_image(device.device(), pixel_count * 3);

  gpu::Kernel raster_kernel(device.device(), device.library(), "raster_forward");
  gpu::dispatch_tiled_and_wait(
      device.queue(), raster_kernel,
      {screen_positions.handle(), covariances.handle(), colors.handle(), opacities.handle(),
       sorted_splat_indices.handle(), tile_range_starts.handle(), tile_range_ends.handle(),
       image_width_buffer.handle(), image_height_buffer.handle(), tiles_x_buffer.handle(),
       out_image.handle()},
      tiles_x, tiles_y, GLINT_TILE_SIZE, GLINT_TILE_SIZE);

  RenderedImage result;
  result.width = camera.width;
  result.height = camera.height;
  result.rgb.assign(out_image.data(), out_image.data() + pixel_count * 3);
  return result;
}

}  // namespace glint::splat
