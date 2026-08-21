#pragma once

#include <Eigen/Core>

#include <cstdint>
#include <vector>

namespace glint::core {

// A camera's fixed calibration: image size and the intrinsics needed to later project a pixel
// + depth value into a 3D point (that projection math itself comes in a later milestone).
struct Camera {
  int width = 0;
  int height = 0;
  float fx = 0.0f;
  float fy = 0.0f;
  float cx = 0.0f;
  float cy = 0.0f;
};

// Where the camera was and which way it faced at one instant, as a camera-to-world transform.
struct Pose {
  Eigen::Matrix4f camera_to_world = Eigen::Matrix4f::Identity();
};

// One captured timestep: the RGB image, the depth image, and the pose the camera had when it
// captured them. rgb is width*height*3 bytes (interleaved RGB); depth is width*height values
// already converted to real-world meters — whatever raw units/scale factor the source dataset
// used gets applied once, during loading, so nothing downstream needs to know about it.
struct Frame {
  std::vector<uint8_t> rgb;
  std::vector<float> depth;
  Pose pose;
};

}  // namespace glint::core
