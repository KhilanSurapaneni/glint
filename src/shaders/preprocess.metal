#include <metal_stdlib>
using namespace metal;

#include "shared_types.h"

// GLINT_TILE_SIZE (shared_types.h) is the single source of truth — this local alias just
// saves typing it out at every use site below.
constant uint kTileSize = GLINT_TILE_SIZE;

// Builds one splat's 3D world-space covariance from its rotation quaternion and per-axis
// scale — mirrors core::covariance_3d exactly; see docs/DERIVATIONS.md for the math this
// implements. `rotation` is normalized here, same "every read normalizes" convention as the
// CPU reference and splat::normalize_quaternion.
float3x3 splat_covariance_3d(float4 rotation, float3 scale) {
  rotation = normalize(rotation);
  float x = rotation.x, y = rotation.y, z = rotation.z, w = rotation.w;

  float3x3 R = float3x3(
      float3(1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + w * z), 2.0 * (x * z - w * y)),
      float3(2.0 * (x * y - w * z), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z + w * x)),
      float3(2.0 * (x * z + w * y), 2.0 * (y * z - w * x), 1.0 - 2.0 * (x * x + y * y)));

  float3x3 S = float3x3(float3(scale.x, 0.0, 0.0), float3(0.0, scale.y, 0.0),
                         float3(0.0, 0.0, scale.z));
  float3x3 M = R * S;
  return M * transpose(M);
}

// Mirrors core::covariance_2d exactly — the EWA projection, see docs/DERIVATIONS.md. Returns
// the symmetric 2x2 result packed as (xx, xy, yy), not the full 3x3 Sigma' — only the
// top-left 2x2 is a valid screen-space footprint, and packing the 3 unique values instead of
// a full matrix halves the bandwidth this kernel's output costs.
float3 splat_covariance_2d(float3x3 world_covariance, float3x3 view_rotation,
                            float3 camera_space_position, float fx, float fy) {
  float x = camera_space_position.x;
  float y = camera_space_position.y;
  float z = camera_space_position.z;

  // Local-linear approximation of the (nonlinear) pinhole projection at this splat's own
  // position — row 2 stays zero, the projected depth's differential isn't part of a 2D
  // footprint.
  float3x3 J = float3x3(0.0);
  J[0][0] = fx / z;
  J[2][0] = -fx * x / (z * z);
  J[1][1] = fy / z;
  J[2][1] = -fy * y / (z * z);

  float3x3 camera_space_covariance = view_rotation * world_covariance * transpose(view_rotation);
  float3x3 sigma_prime = J * camera_space_covariance * transpose(J);

  return float3(sigma_prime[0][0], sigma_prime[0][1], sigma_prime[1][1]);
}

// --- Spherical harmonics: mirrors core/spherical_harmonics.hpp exactly ---

constant float kShC0 = 0.28209479177387814;
constant float kShC1 = 0.4886025119029199;
constant float kShC2[5] = {1.0925484305920792, -1.0925484305920792, 0.31539156525252005,
                            -1.0925484305920792, 0.5462742152960396};
constant float kShC3[7] = {-0.5900435899266435, 2.890611442640554, -0.4570457994644658,
                            0.3731763325901154, -0.4570457994644658, 1.445305721320277,
                            -0.5900435899266435};

float3 evaluate_sh_color(device const float* sh, float3 direction) {
  float x = direction.x, y = direction.y, z = direction.z;
  float xx = x * x, yy = y * y, zz = z * z;
  float xy = x * y, yz = y * z, xz = x * z;

  float b[16];
  b[0] = kShC0;
  b[1] = -kShC1 * y;
  b[2] = kShC1 * z;
  b[3] = -kShC1 * x;
  b[4] = kShC2[0] * xy;
  b[5] = kShC2[1] * yz;
  b[6] = kShC2[2] * (2.0 * zz - xx - yy);
  b[7] = kShC2[3] * xz;
  b[8] = kShC2[4] * (xx - yy);
  b[9] = kShC3[0] * y * (3.0 * xx - yy);
  b[10] = kShC3[1] * xy * z;
  b[11] = kShC3[2] * y * (4.0 * zz - xx - yy);
  b[12] = kShC3[3] * z * (2.0 * zz - 3.0 * xx - 3.0 * yy);
  b[13] = kShC3[4] * x * (4.0 * zz - xx - yy);
  b[14] = kShC3[5] * z * (xx - yy);
  b[15] = kShC3[6] * x * (xx - 3.0 * yy);

  float3 color = float3(0.5);
  for (int channel = 0; channel < 3; ++channel) {
    float sum = 0.0;
    for (int k = 0; k < 16; ++k) {
      sum += sh[channel * 16 + k] * b[k];
    }
    color[channel] += sum;
  }
  return color;
}

// One thread per splat: frustum cull, project onto screen, compute 2D covariance,
// view-dependent color, and decoded opacity, and figure out which screen tiles this splat
// touches (for Phase 3's sort to size itself against). Embarrassingly parallel — no
// threadgroup memory needed, every thread's work is fully independent.
kernel void preprocess_splats(device const float* positions [[buffer(0)]],
                               device const float* rotations [[buffer(1)]],
                               device const float* log_scales [[buffer(2)]],
                               device const float* sh_coeffs [[buffer(3)]],
                               device const float* logit_opacities [[buffer(4)]],
                               constant PreprocessParams& params [[buffer(5)]],
                               device float* out_screen_positions [[buffer(6)]],
                               device float* out_covariances [[buffer(7)]],
                               device float* out_colors [[buffer(8)]],
                               device float* out_opacities [[buffer(9)]],
                               device float* out_radii [[buffer(10)]],
                               device uint* out_tile_touch_counts [[buffer(11)]],
                               device float* out_depths [[buffer(12)]],
                               uint id [[thread_position_in_grid]]) {
  float3 world_position =
      float3(positions[id * 3 + 0], positions[id * 3 + 1], positions[id * 3 + 2]);
  float4 camera_space = params.view_matrix * float4(world_position, 1.0);

  // Behind the camera (or right at it): can't be projected. Mark as touching no tiles so
  // Phase 3's sort naturally skips it, without a separate "is this splat valid" flag anywhere.
  if (camera_space.z <= 1e-4) {
    out_tile_touch_counts[id] = 0;
    return;
  }

  float3x3 view_rotation = float3x3(params.view_matrix[0].xyz, params.view_matrix[1].xyz,
                                     params.view_matrix[2].xyz);

  float4 rotation = float4(rotations[id * 4 + 0], rotations[id * 4 + 1],
                            rotations[id * 4 + 2], rotations[id * 4 + 3]);
  float3 scale = float3(exp(log_scales[id * 3 + 0]), exp(log_scales[id * 3 + 1]),
                         exp(log_scales[id * 3 + 2]));

  float3x3 sigma_3d = splat_covariance_3d(rotation, scale);
  float3 sigma_2d =
      splat_covariance_2d(sigma_3d, view_rotation, camera_space.xyz, params.fx, params.fy);

  // +0.3 anti-aliasing low-pass filter, same as core::apply_antialiasing_filter. sigma_2d
  // packs (xx, xy, yy), so the diagonal entries are .x and .z.
  sigma_2d.x += 0.3;
  sigma_2d.z += 0.3;

  // Saved for Phase 3's sort — depth is what determines front-to-back draw order, and once
  // this kernel returns there's no other way to recover it (the screen position alone can't
  // tell you how far away a splat was, only where it landed).
  out_depths[id] = camera_space.z;

  float screen_x = params.fx * camera_space.x / camera_space.z + params.cx;
  float screen_y = params.fy * camera_space.y / camera_space.z + params.cy;

  out_screen_positions[id * 2 + 0] = screen_x;
  out_screen_positions[id * 2 + 1] = screen_y;
  out_covariances[id * 3 + 0] = sigma_2d.x;
  out_covariances[id * 3 + 1] = sigma_2d.y;
  out_covariances[id * 3 + 2] = sigma_2d.z;

  float3 camera_position =
      float3(params.camera_position_x, params.camera_position_y, params.camera_position_z);
  float3 view_direction = normalize(world_position - camera_position);
  float3 color = evaluate_sh_color(sh_coeffs + id * 48, view_direction);
  out_colors[id * 3 + 0] = color.x;
  out_colors[id * 3 + 1] = color.y;
  out_colors[id * 3 + 2] = color.z;

  // Decoded here (not left raw) for the same reason color and scale already are — the
  // raster kernel reads a ready-to-use value, never a raw SplatSoA field directly.
  out_opacities[id] = 1.0 / (1.0 + exp(-logit_opacities[id]));

  // Screen-space radius: 3 standard deviations along the covariance's largest axis — same
  // closed-form 2x2 eigenvalue formula as core::screen_space_radius.
  float mid = 0.5 * (sigma_2d.x + sigma_2d.z);
  float det = sigma_2d.x * sigma_2d.z - sigma_2d.y * sigma_2d.y;
  float discriminant = max(mid * mid - det, 0.0);
  float largest_eigenvalue = mid + sqrt(discriminant);
  float radius = 3.0 * sqrt(max(largest_eigenvalue, 0.0));
  out_radii[id] = radius;

  // Off-screen entirely, even though it's in front of the camera (e.g. projected far to one
  // side) -- no tiles to touch.
  if (screen_x + radius < 0.0 || screen_x - radius > float(params.image_width) ||
      screen_y + radius < 0.0 || screen_y - radius > float(params.image_height)) {
    out_tile_touch_counts[id] = 0;
    return;
  }

  uint tiles_x = (params.image_width + kTileSize - 1) / kTileSize;
  uint tiles_y = (params.image_height + kTileSize - 1) / kTileSize;

  int min_tile_x = clamp(int((screen_x - radius) / float(kTileSize)), 0, int(tiles_x) - 1);
  int max_tile_x = clamp(int((screen_x + radius) / float(kTileSize)), 0, int(tiles_x) - 1);
  int min_tile_y = clamp(int((screen_y - radius) / float(kTileSize)), 0, int(tiles_y) - 1);
  int max_tile_y = clamp(int((screen_y + radius) / float(kTileSize)), 0, int(tiles_y) - 1);

  out_tile_touch_counts[id] =
      uint(max_tile_x - min_tile_x + 1) * uint(max_tile_y - min_tile_y + 1);
}
