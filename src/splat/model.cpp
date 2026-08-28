#include "splat/model.hpp"

#include "gpu/kernel.hpp"
#include "shaders/shared_types.h"

#include <algorithm>
#include <cstdint>

namespace glint::splat {

namespace {

// One pooled point, kept only long enough to build SplatSoA from it — not stored anywhere
// long-term, unlike core::Frame which owns the actual image data.
struct UnprojectedPoint {
  float x, y, z;
  float r, g, b;  // 0..1
  float depth;    // needed for the scale-from-pixel-footprint estimate below
};

// A modest, non-degenerate starting opacity — solid enough to actually contribute to early
// renders, far enough from 0 or 1 that its gradient isn't already near-saturated (see the
// sigmoid-saturation discussion in tests/test_splat_model.cpp).
constexpr float kInitialOpacity = 0.1f;

// Unprojects every frame's depth on the GPU (M1's kernel, unchanged) and pools the results —
// the only GPU work in this file; everything after this is cheap per-point bookkeeping that
// doesn't warrant its own kernel.
std::vector<UnprojectedPoint> unproject_all_frames(gpu::Device& device,
                                                     const std::vector<core::Frame>& frames,
                                                     const core::Camera& camera) {
  const size_t pixel_count = static_cast<size_t>(camera.width) * camera.height;

  gpu::Buffer<uint32_t> image_width_buffer(device.device(), 1);
  image_width_buffer.data()[0] = static_cast<uint32_t>(camera.width);

  gpu::Kernel unproject_kernel(device.device(), device.library(), "unproject_pixel");

  std::vector<UnprojectedPoint> points;
  points.reserve(pixel_count * frames.size());

  for (const core::Frame& frame : frames) {
    gpu::Buffer<float> depth_buffer(device.device(), pixel_count);
    std::copy(frame.depth.begin(), frame.depth.end(), depth_buffer.data());

    gpu::Buffer<uint8_t> rgb_buffer(device.device(), pixel_count * 3);
    std::copy(frame.rgb.begin(), frame.rgb.end(), rgb_buffer.data());

    UnprojectParams params{};
    for (int col = 0; col < 4; ++col) {
      for (int row = 0; row < 4; ++row) {
        params.camera_to_world.columns[col][row] = frame.pose.camera_to_world(row, col);
      }
    }
    params.fx = camera.fx;
    params.fy = camera.fy;
    params.cx = camera.cx;
    params.cy = camera.cy;

    gpu::Buffer<UnprojectParams> params_buffer(device.device(), 1);
    params_buffer.data()[0] = params;

    gpu::Buffer<float> positions_buffer(device.device(), pixel_count * 3);
    gpu::Buffer<float> colors_buffer(device.device(), pixel_count * 3);

    gpu::dispatch_and_wait(
        device.queue(), unproject_kernel,
        {depth_buffer.handle(), rgb_buffer.handle(), params_buffer.handle(),
         image_width_buffer.handle(), positions_buffer.handle(), colors_buffer.handle()},
        pixel_count);

    for (size_t i = 0; i < pixel_count; ++i) {
      points.push_back(UnprojectedPoint{
          positions_buffer.data()[i * 3 + 0],
          positions_buffer.data()[i * 3 + 1],
          positions_buffer.data()[i * 3 + 2],
          colors_buffer.data()[i * 3 + 0],
          colors_buffer.data()[i * 3 + 1],
          colors_buffer.data()[i * 3 + 2],
          depth_buffer.data()[i],
      });
    }
  }

  return points;
}

}  // namespace

SplatSoA initialize_from_frames(gpu::Device& device, const std::vector<core::Frame>& frames,
                                 const core::Camera& camera, size_t target_splat_count) {
  const std::vector<UnprojectedPoint> points = unproject_all_frames(device, frames, camera);

  // Every Nth point, evenly spread through the whole pool rather than a prefix — the pool was
  // built frame by frame, so a prefix would just be "the first few frames' points," not a
  // representative sample of everything unprojected.
  const size_t stride =
      std::max<size_t>(1, points.size() / std::max<size_t>(1, target_splat_count));
  size_t selected_count = 0;
  for (size_t i = 0; i < points.size(); i += stride) {
    ++selected_count;
  }

  SplatSoA splats(device.device(), selected_count);
  const float logit_initial_opacity = logit_from_opacity(kInitialOpacity);

  size_t splat_index = 0;
  for (size_t i = 0; i < points.size(); i += stride) {
    const UnprojectedPoint& point = points[i];

    splats.positions.data()[splat_index * 3 + 0] = point.x;
    splats.positions.data()[splat_index * 3 + 1] = point.y;
    splats.positions.data()[splat_index * 3 + 2] = point.z;

    // Identity: no rotation. There's no information yet about this point's true surface
    // orientation — training corrects it from here.
    splats.rotations.data()[splat_index * 4 + 0] = 0.0f;
    splats.rotations.data()[splat_index * 4 + 1] = 0.0f;
    splats.rotations.data()[splat_index * 4 + 2] = 0.0f;
    splats.rotations.data()[splat_index * 4 + 3] = 1.0f;

    // A pixel's real-world footprint at distance `depth` is approximately depth/fx (fx is the
    // focal length in pixels — how much real-world space one pixel spans at that distance;
    // Replica's fx == fy, so using fx alone doesn't lose anything here). Sizing the splat to
    // roughly that footprint means neighboring splats approximately touch without large gaps
    // or heavy overlap — without an expensive nearest-neighbor search over the point cloud.
    const float footprint = std::max(point.depth / camera.fx, 1e-4f);  // floor: avoid log(0)
    const float log_scale = log_from_scale(footprint);
    splats.log_scales.data()[splat_index * 3 + 0] = log_scale;
    splats.log_scales.data()[splat_index * 3 + 1] = log_scale;
    splats.log_scales.data()[splat_index * 3 + 2] = log_scale;

    splats.logit_opacities.data()[splat_index] = logit_initial_opacity;

    // Degree-0 (flat, non-view-dependent) SH term from this point's actual color; every
    // higher-degree (view-dependent) term starts at zero — training discovers view-dependence
    // if the data actually shows it.
    // Inverse of core::evaluate_sh_color's degree-0 term (0.5 + kShC0 * sh0) — see
    // core/spherical_harmonics.hpp.
    float* sh = &splats.sh_coeffs.data()[splat_index * kShCoeffs];
    sh[0 * kShCoeffsPerChannel] = (point.r - 0.5f) / core::kShC0;
    sh[1 * kShCoeffsPerChannel] = (point.g - 0.5f) / core::kShC0;
    sh[2 * kShCoeffsPerChannel] = (point.b - 0.5f) / core::kShC0;
    for (int channel = 0; channel < 3; ++channel) {
      for (int coeff = 1; coeff < kShCoeffsPerChannel; ++coeff) {
        sh[channel * kShCoeffsPerChannel + coeff] = 0.0f;
      }
    }

    ++splat_index;
  }

  return splats;
}

}  // namespace glint::splat
