#pragma once

#include <simd/simd.h>

namespace glint::viewer {

// A camera for freely looking around a rendered scene — distinct from core::Pose, which
// describes where the *capturing* camera was when a frame was recorded. This one describes
// how *we* currently choose to look at the result. Orbits around a fixed target point at a
// given distance/yaw/pitch, updated live by mouse drag (orbit) and scroll (zoom).
class OrbitCamera {
 public:
  // The combined view * projection matrix, ready to feed straight into a vertex shader.
  // `aspect_ratio` is viewport width / height.
  simd::float4x4 view_projection_matrix(float aspect_ratio) const;

  // Called from a mouse-drag delta, in radians. Pitch is clamped so the camera can't flip
  // upside down (looking straight up/down loses a stable sense of "up").
  void orbit(float delta_yaw, float delta_pitch);

  // Called from a scroll delta. Clamped so you can't zoom through the target or out to
  // an unusably large distance.
  void zoom(float delta_distance);

 private:
  // Defaults matched to Replica room0's actual measured extent (frame 0 spans roughly
  // x[3.45,7.84] y[-0.58,5.09] z[0.49,2.71] — nowhere near the coordinate origin). These are
  // dataset-specific guesses, not a general solution — real mouse-driven framing in 1.4 is
  // what actually fixes this properly for arbitrary scenes.
  simd::float3 target_ = {5.6f, 2.25f, 1.6f};
  float distance_ = 8.0f;
  float yaw_ = 0.6f;    // radians, around the vertical axis
  float pitch_ = 0.4f;  // radians, tilt up/down
};

}  // namespace glint::viewer
