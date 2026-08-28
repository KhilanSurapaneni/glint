#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>

namespace glint::core {

// A splat's world-space "shape" as a 3x3 covariance matrix, built from its rotation and
// per-axis scale: Sigma = R * S * S^T * R^T (Kerbl et al. 2023 eq. 6), where R is the
// rotation matrix and S = diag(sx, sy, sz). See docs/DERIVATIONS.md for why this
// construction — rotate an axis-aligned ellipsoid — always produces a valid covariance no
// matter what rotation/scale go in, which is the whole reason scale/rotation are stored
// instead of Sigma's 6 numbers directly.
//
// `rotation` is normalized internally (every *read* normalizes, same convention as
// splat::normalize_quaternion) — the caller doesn't need to normalize it first.
inline Eigen::Matrix3f covariance_3d(const Eigen::Quaternionf& rotation,
                                      const Eigen::Vector3f& scale) {
  const Eigen::Matrix3f R = rotation.normalized().toRotationMatrix();
  // M = R*S rather than building S*S^T separately first — mathematically identical
  // (S^T == S since S is diagonal), one fewer matrix multiply.
  const Eigen::Matrix3f M = R * scale.asDiagonal();
  return M * M.transpose();
}

// Projects a 3D world-space covariance into a 2D screen-space covariance for one splat — the
// EWA projection (Zwicker et al. 2002; full derivation, including why the affine/Jacobian
// approximation below is only valid locally around the splat, in docs/DERIVATIONS.md).
//
// `view_rotation` is the 3x3 rotation part of the world-to-camera transform (translation
// doesn't affect covariance, only rotation does). `camera_space_position` is this splat's
// mean already transformed into camera space — needed because the Jacobian is evaluated at
// that specific point, not a fixed formula. Returns the top-left 2x2 block of
// Sigma' = J W Sigma W^T J^T — the only part that describes actual on-screen (x, y) spread.
inline Eigen::Matrix2f covariance_2d(const Eigen::Matrix3f& world_covariance,
                                      const Eigen::Matrix3f& view_rotation,
                                      const Eigen::Vector3f& camera_space_position, float fx,
                                      float fy) {
  const float x = camera_space_position.x();
  const float y = camera_space_position.y();
  const float z = camera_space_position.z();

  // Local-linear approximation of the (nonlinear, divides by z) pinhole projection, evaluated
  // right at this splat's own position. Row 2 stays zero — the projected depth's own
  // differential isn't part of a 2D footprint, so it's dropped rather than computed.
  Eigen::Matrix3f J = Eigen::Matrix3f::Zero();
  J(0, 0) = fx / z;
  J(0, 2) = -fx * x / (z * z);
  J(1, 1) = fy / z;
  J(1, 2) = -fy * y / (z * z);

  const Eigen::Matrix3f camera_space_covariance =
      view_rotation * world_covariance * view_rotation.transpose();
  const Eigen::Matrix3f sigma_prime = J * camera_space_covariance * J.transpose();

  return sigma_prime.topLeftCorner<2, 2>();
}

// A fixed low-pass filter: without this, splats smaller than about a pixel flicker in and out
// of existence as the camera moves slightly — the same aliasing a thin wire shows between two
// nearby camera positions in a photo. Applied after projection, not baked into the 3D
// covariance, since it's purely a screen-space anti-aliasing measure.
inline constexpr float kAntiAliasingRegularizer = 0.3f;

inline Eigen::Matrix2f apply_antialiasing_filter(Eigen::Matrix2f covariance) {
  covariance(0, 0) += kAntiAliasingRegularizer;
  covariance(1, 1) += kAntiAliasingRegularizer;
  return covariance;
}

// How far (in pixels) a splat's influence meaningfully reaches on screen — 3 standard
// deviations along its *largest* axis, i.e. a circle big enough to contain the ellipse from
// any orientation. For a symmetric 2x2 matrix [[a,b],[b,c]], the eigenvalues have a closed
// form: (a+c)/2 +/- sqrt(((a+c)/2)^2 - det) — no iterative eigensolver needed. Used to decide
// which screen tiles a splat touches (Phase 3), so it needs to be a safe over-estimate, not a
// tight fit.
inline float screen_space_radius(const Eigen::Matrix2f& covariance_2d) {
  const float a = covariance_2d(0, 0);
  const float b = covariance_2d(0, 1);
  const float c = covariance_2d(1, 1);

  const float mid = 0.5f * (a + c);
  const float det = a * c - b * b;
  const float discriminant = std::max(mid * mid - det, 0.0f);
  const float largest_eigenvalue = mid + std::sqrt(discriminant);

  return 3.0f * std::sqrt(std::max(largest_eigenvalue, 0.0f));
}

}  // namespace glint::core
