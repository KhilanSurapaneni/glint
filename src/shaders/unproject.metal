#include <metal_stdlib>
using namespace metal;

#include "shared_types.h"

// Turns one pixel + its depth value into a 3D world-space point + color. One thread per
// pixel; (u, v) is recovered from the flat thread index using the image width.
//
// Pinhole camera model (inverse direction — pixel+depth -> 3D point, not 3D point -> pixel):
//   x = (u - cx) * d / fx
//   y = (v - cy) * d / fy
//   z = d
// That gives a point in camera space; multiplying by camera_to_world moves it into the shared
// world coordinate system so points from different frames/poses line up correctly.
kernel void unproject_pixel(device const float* depth [[buffer(0)]],
                             device const uint8_t* rgb [[buffer(1)]],
                             constant UnprojectParams& params [[buffer(2)]],
                             constant uint& width [[buffer(3)]],
                             device float* out_positions [[buffer(4)]],
                             device float* out_colors [[buffer(5)]],
                             uint index [[thread_position_in_grid]]) {
  uint u = index % width;
  uint v = index / width;

  float d = depth[index];

  float x = (float(u) - params.cx) * d / params.fx;
  float y = (float(v) - params.cy) * d / params.fy;
  float z = d;

  float4 world_point = params.camera_to_world * float4(x, y, z, 1.0);

  out_positions[index * 3 + 0] = world_point.x;
  out_positions[index * 3 + 1] = world_point.y;
  out_positions[index * 3 + 2] = world_point.z;

  out_colors[index * 3 + 0] = float(rgb[index * 3 + 0]) / 255.0;
  out_colors[index * 3 + 1] = float(rgb[index * 3 + 1]) / 255.0;
  out_colors[index * 3 + 2] = float(rgb[index * 3 + 2]) / 255.0;
}
