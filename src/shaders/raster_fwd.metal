#include <metal_stdlib>
using namespace metal;

#include "shared_types.h"

// One threadgroup per 16x16 screen tile, one thread per pixel within it — the fixed sizing
// every stage of this pipeline (preprocess's tile-touch counting, the sort's tile grouping)
// already assumes. NOTE: this version reads each splat directly from device memory per
// thread, rather than cooperatively loading batches into threadgroup memory first (the
// standard 3DGS optimization). Same result, more redundant memory traffic — deliberately
// simple for now; see docs/M2_PLAN.md Phase 4 for why correctness comes before that
// optimization here, same reasoning as Phase 3's CPU-first sort.
kernel void raster_forward(device const float* screen_positions [[buffer(0)]],   // 2/splat
                            device const float* covariances [[buffer(1)]],       // 3/splat (xx,xy,yy)
                            device const float* colors [[buffer(2)]],            // 3/splat
                            device const float* opacities [[buffer(3)]],         // 1/splat, decoded
                            device const uint* sorted_splat_indices [[buffer(4)]],
                            device const uint* tile_range_starts [[buffer(5)]],
                            device const uint* tile_range_ends [[buffer(6)]],
                            constant uint& image_width [[buffer(7)]],
                            constant uint& image_height [[buffer(8)]],
                            constant uint& tiles_x [[buffer(9)]],
                            device float* out_image [[buffer(10)]],  // 3/pixel, row-major RGB
                            uint2 pixel [[thread_position_in_grid]],
                            uint2 threadgroup_position [[threadgroup_position_in_grid]]) {
  // A 680px-tall image at 16px tiles has a partial last row of tiles (680 = 42*16 + 8) — any
  // thread whose pixel falls past the real image edge has nothing to do.
  if (pixel.x >= image_width || pixel.y >= image_height) {
    return;
  }

  const uint tile_id = threadgroup_position.y * tiles_x + threadgroup_position.x;
  const uint start = tile_range_starts[tile_id];
  const uint end = tile_range_ends[tile_id];

  // Pixel *centers*, not corners — a splat exactly covering pixel 0 should contribute fully
  // to the sample taken at its middle, not its top-left corner.
  const float2 pixel_center = float2(float(pixel.x) + 0.5, float(pixel.y) + 0.5);

  float3 accumulated_color = float3(0.0);
  float transmittance = 1.0;

  for (uint i = start; i < end; ++i) {
    if (transmittance < 1e-4) {
      break;  // remaining splats' contribution is imperceptible — front-to-back order is what
              // makes this early-out both correct and effective, not just a speed hack
    }

    const uint splat_index = sorted_splat_indices[i];

    const float2 splat_screen_position =
        float2(screen_positions[splat_index * 2 + 0], screen_positions[splat_index * 2 + 1]);
    const float2 delta = pixel_center - splat_screen_position;

    const float cov_xx = covariances[splat_index * 3 + 0];
    const float cov_xy = covariances[splat_index * 3 + 1];
    const float cov_yy = covariances[splat_index * 3 + 2];

    // Squared Mahalanobis distance needs the covariance's *inverse* — closed-form for a 2x2.
    const float det = cov_xx * cov_yy - cov_xy * cov_xy;
    if (det <= 0.0) {
      continue;  // degenerate shape (shouldn't happen given the +0.3 regularizer) — skip, not NaN
    }
    const float inv_det = 1.0 / det;
    const float inv_xx = cov_yy * inv_det;
    const float inv_xy = -cov_xy * inv_det;
    const float inv_yy = cov_xx * inv_det;

    const float mahalanobis_sq = delta.x * delta.x * inv_xx + 2.0 * delta.x * delta.y * inv_xy +
                                  delta.y * delta.y * inv_yy;

    float alpha = opacities[splat_index] * exp(-0.5 * mahalanobis_sq);
    alpha = min(alpha, 0.99);  // numerical safety margin, avoids alpha reaching exactly 1

    if (alpha < 1.0 / 255.0) {
      continue;  // negligible contribution to this specific pixel — not worth accumulating
    }

    const float3 splat_color =
        float3(colors[splat_index * 3 + 0], colors[splat_index * 3 + 1], colors[splat_index * 3 + 2]);

    accumulated_color += transmittance * alpha * splat_color;
    transmittance *= (1.0 - alpha);
  }

  const uint pixel_index = pixel.y * image_width + pixel.x;
  out_image[pixel_index * 3 + 0] = accumulated_color.x;
  out_image[pixel_index * 3 + 1] = accumulated_color.y;
  out_image[pixel_index * 3 + 2] = accumulated_color.z;
}
