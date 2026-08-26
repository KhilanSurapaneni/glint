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
