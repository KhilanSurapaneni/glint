#include <metal_stdlib>
using namespace metal;

struct VertexOut {
  float4 position [[position]];  // where this vertex lands on screen
  float3 color;
};

// The triangle's 3 corners, hardcoded for now — a real vertex buffer comes in a later phase.
constant float2 kPositions[3] = {
  float2(0.0, 0.5),
  float2(-0.5, -0.5),
  float2(0.5, -0.5),
};

constant float3 kColors[3] = {
  float3(1.0, 0.0, 0.0),
  float3(0.0, 1.0, 0.0),
  float3(0.0, 0.0, 1.0),
};

// runs once per vertex (3 for triangle)
vertex VertexOut triangle_vertex(uint vertex_id [[vertex_id]]) {
  VertexOut out;
  out.position = float4(kPositions[vertex_id], 0.0, 1.0);
  out.color = kColors[vertex_id];
  return out;
}

// runs once per every pixel the triangle covers on the screen. in.color arrives already
// blended between the 3 vertex colors — that's automatic GPU interpolation, not computed here.
fragment float4 triangle_fragment(VertexOut in [[stage_in]]) {
  return float4(in.color, 1.0);
}
