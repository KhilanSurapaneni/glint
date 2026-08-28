#include <metal_stdlib>
using namespace metal;

#include "shared_types.h"

// Copies every field of UnprojectParams into a flat array of floats, so tests/test_layout.cpp
// can check the GPU read the struct's memory layout correctly. Not used outside this test.
kernel void read_unproject_params(device const UnprojectParams* params [[buffer(0)]],
                                   device float* out [[buffer(1)]]) {
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      out[col * 4 + row] = params->camera_to_world[col][row];
    }
  }
  out[16] = params->fx;
  out[17] = params->fy;
  out[18] = params->cx;
  out[19] = params->cy;
}

// Same purpose as read_unproject_params, for PreprocessParams.
kernel void read_preprocess_params(device const PreprocessParams* params [[buffer(0)]],
                                    device float* out [[buffer(1)]]) {
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      out[col * 4 + row] = params->view_matrix[col][row];
    }
  }
  out[16] = params->camera_position_x;
  out[17] = params->camera_position_y;
  out[18] = params->camera_position_z;
  out[19] = params->fx;
  out[20] = params->fy;
  out[21] = params->cx;
  out[22] = params->cy;
  out[23] = float(params->image_width);
  out[24] = float(params->image_height);
}
