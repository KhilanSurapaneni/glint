#pragma once

// __METAL_VERSION__ is only defined when this header is compiled by the Metal shader compiler
// — this lets one header provide the right type name on each side of the boundary while
// staying layout-identical: MSL's own float4x4 here, Apple's simd:: equivalent in plain C++.
#ifndef __METAL_VERSION__
#include <simd/simd.h>
#endif

// Structs shared between C++ and Metal Shading Language go here. MSL alignment rules differ
// from C++ (e.g. float3 is 16-byte aligned/sized, not 12). Every struct added to this file
// needs a static_assert(sizeof/alignof) on the C++ side and a round-trip check in
// tests/test_layout.cpp before it's trusted.

// The screen-tile size the preprocess, sort, and raster stages all agree on — a #define
// (not a `constant uint`, which is Metal-only syntax) so it compiles identically on both
// sides of the boundary and can never silently drift between the kernels that all assume it's
// the same value.
#define GLINT_TILE_SIZE 16

// Intrinsics + pose the unprojection kernel needs to turn a pixel + depth value into a 3D
// world-space point.
struct UnprojectParams {
#ifdef __METAL_VERSION__
  float4x4 camera_to_world;
#else
  simd::float4x4 camera_to_world;
#endif
  float fx;
  float fy;
  float cx;
  float cy;
};

#ifndef __METAL_VERSION__
static_assert(sizeof(UnprojectParams) == 80, "UnprojectParams size must match its MSL layout");
static_assert(alignof(UnprojectParams) == 16, "UnprojectParams alignment must match its MSL layout");
#endif

// Everything the preprocess kernel (Phase 2) needs about the current camera view: how the
// world transforms into camera space, where the camera itself is (needed separately, for the
// SH view direction — deriving it from view_matrix would mean inverting a matrix on every GPU
// thread for a value that's the same for the whole dispatch), intrinsics, and image size (for
// tile-touch counting).
struct PreprocessParams {
#ifdef __METAL_VERSION__
  float4x4 view_matrix;  // world-to-camera — NOT camera-to-world, unlike UnprojectParams
#else
  simd::float4x4 view_matrix;
#endif
  float camera_position_x;
  float camera_position_y;
  float camera_position_z;
  float fx;
  float fy;
  float cx;
  float cy;
  uint32_t image_width;
  uint32_t image_height;
};

#ifndef __METAL_VERSION__
static_assert(sizeof(PreprocessParams) == 112, "PreprocessParams size must match its MSL layout");
static_assert(alignof(PreprocessParams) == 16,
              "PreprocessParams alignment must match its MSL layout");
#endif
