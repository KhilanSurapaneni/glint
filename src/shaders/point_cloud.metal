#include <metal_stdlib>
using namespace metal;

struct PointOut {
  float4 position [[position]];
  float point_size [[point_size]];
  float3 color;
};

// Reads one real 3D point + color (built by unproject_pixel) from flat buffers, transforms it
// into the current view, and sets how big the rasterizer should draw the resulting dot.
vertex PointOut point_vertex(uint vertex_id [[vertex_id]],
                              device const float* positions [[buffer(0)]],
                              device const float* colors [[buffer(1)]],
                              constant float4x4& view_projection [[buffer(2)]]) {
  float3 world_position = float3(positions[vertex_id * 3 + 0], positions[vertex_id * 3 + 1],
                                  positions[vertex_id * 3 + 2]);

  PointOut out;
  out.position = view_projection * float4(world_position, 1.0);
  out.point_size = 4.0;
  out.color =
      float3(colors[vertex_id * 3 + 0], colors[vertex_id * 3 + 1], colors[vertex_id * 3 + 2]);
  return out;
}

fragment float4 point_fragment(PointOut in [[stage_in]]) {
  return float4(in.color, 1.0);
}
