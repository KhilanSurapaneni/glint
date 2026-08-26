#include "viewer/orbit_camera.hpp"

#include <algorithm>
#include <cmath>

namespace glint::viewer {

namespace {

// Standard lookAt construction: builds a matrix that transforms world-space points into the
// camera's own space (camera at the origin, looking down -Z).
simd::float4x4 look_at(simd::float3 eye, simd::float3 target, simd::float3 up) {
  const simd::float3 forward = simd::normalize(target - eye);
  const simd::float3 right = simd::normalize(simd::cross(forward, up));
  const simd::float3 true_up = simd::cross(right, forward);

  return simd::float4x4(
      simd::float4{right.x, true_up.x, -forward.x, 0.0f},
      simd::float4{right.y, true_up.y, -forward.y, 0.0f},
      simd::float4{right.z, true_up.z, -forward.z, 0.0f},
      simd::float4{-simd::dot(right, eye), -simd::dot(true_up, eye), simd::dot(forward, eye),
                   1.0f});
}

// Metal's clip space maps the near plane to z=0 and the far plane to z=1 — unlike OpenGL's
// -1..1 range. Getting this wrong doesn't crash, it just clips/culls things unexpectedly.
simd::float4x4 perspective(float fov_y_radians, float aspect_ratio, float near_z, float far_z) {
  const float y_scale = 1.0f / std::tan(fov_y_radians * 0.5f);
  const float x_scale = y_scale / aspect_ratio;
  const float z_scale = far_z / (far_z - near_z);

  // w must come out positive for points in front of the camera (negative view-space Z, since
  // the camera looks down -Z) — that's why the third and fourth rows are negated relative to
  // a naive derivation. Getting this backwards clips away every point in the scene silently.
  return simd::float4x4(simd::float4{x_scale, 0.0f, 0.0f, 0.0f},
                         simd::float4{0.0f, y_scale, 0.0f, 0.0f},
                         simd::float4{0.0f, 0.0f, -z_scale, -1.0f},
                         simd::float4{0.0f, 0.0f, -near_z * z_scale, 0.0f});
}

}  // namespace

simd::float4x4 OrbitCamera::view_projection_matrix(float aspect_ratio) const {
  // Spherical coordinates around the target: distance/yaw/pitch -> an eye position.
  const simd::float3 eye = target_ + distance_ * simd::float3{
                                          std::cos(pitch_) * std::sin(yaw_),
                                          std::sin(pitch_),
                                          std::cos(pitch_) * std::cos(yaw_),
                                      };

  const simd::float4x4 view = look_at(eye, target_, simd::float3{0.0f, 1.0f, 0.0f});
  const simd::float4x4 proj = perspective(/*fov_y=*/1.0f, aspect_ratio, /*near=*/0.05f,
                                           /*far=*/100.0f);
  return proj * view;
}

void OrbitCamera::orbit(float delta_yaw, float delta_pitch) {
  yaw_ += delta_yaw;

  // Clamp just short of straight up/down — exactly vertical loses a stable "up" direction,
  // which would make the view spin unpredictably.
  constexpr float kMaxPitch = 1.5f;
  pitch_ = std::clamp(pitch_ + delta_pitch, -kMaxPitch, kMaxPitch);
}

void OrbitCamera::zoom(float delta_distance) {
  constexpr float kMinDistance = 0.5f;
  constexpr float kMaxDistance = 50.0f;
  distance_ = std::clamp(distance_ + delta_distance, kMinDistance, kMaxDistance);
}

}  // namespace glint::viewer
