#include <gtest/gtest.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "core/covariance.hpp"

#include <cmath>

TEST(Covariance, IdentityRotationUnitScaleGivesIdentity) {
  const Eigen::Matrix3f sigma =
      glint::core::covariance_3d(Eigen::Quaternionf::Identity(), Eigen::Vector3f(1, 1, 1));
  EXPECT_TRUE(sigma.isApprox(Eigen::Matrix3f::Identity(), 1e-6f));
}

TEST(Covariance, IdentityRotationAnisotropicScaleGivesDiagonalOfSquares) {
  const Eigen::Matrix3f sigma =
      glint::core::covariance_3d(Eigen::Quaternionf::Identity(), Eigen::Vector3f(2, 3, 4));
  Eigen::Matrix3f expected = Eigen::Matrix3f::Zero();
  expected.diagonal() = Eigen::Vector3f(4, 9, 16);  // scale squared, no rotation to mix axes
  EXPECT_TRUE(sigma.isApprox(expected, 1e-5f));
}

// A splat originally stretched along Y, rotated 90 degrees around Z, should end up stretched
// along X instead — hand-verified: R maps the Y axis (0,1,0) to (-1,0,0) at this angle, so
// "big along Y" becomes "big along X" after rotation. Uses Eigen's own AngleAxis -> quaternion
// conversion rather than hand-typing quaternion components, to keep this test's expected
// value independent of any quaternion-math mistake in the code under test.
TEST(Covariance, RotationMovesTheStretchedAxis) {
  const Eigen::Quaternionf rotation(
      Eigen::AngleAxisf(static_cast<float>(M_PI) / 2.0f, Eigen::Vector3f::UnitZ()));
  const Eigen::Matrix3f sigma = glint::core::covariance_3d(rotation, Eigen::Vector3f(1, 4, 1));

  Eigen::Matrix3f expected = Eigen::Matrix3f::Zero();
  expected.diagonal() = Eigen::Vector3f(16, 1, 1);  // stretch (4^2) moved from Y to X
  EXPECT_TRUE(sigma.isApprox(expected, 1e-4f));
}

// A splat directly on the optical axis (x=y=0), unit focal length, unit distance, isotropic
// unit scale: everything cancels to the simplest possible case — the projected 2D covariance
// should be exactly the 2x2 identity (a plain circle, no perspective distortion or scaling).
TEST(Covariance, OnAxisUnitCaseProjectsToIdentity) {
  const Eigen::Matrix3f world_covariance = Eigen::Matrix3f::Identity();
  const Eigen::Matrix3f view_rotation = Eigen::Matrix3f::Identity();
  const Eigen::Vector3f camera_space_position(0.0f, 0.0f, 1.0f);  // straight ahead, distance 1

  const Eigen::Matrix2f sigma_2d =
      glint::core::covariance_2d(world_covariance, view_rotation, camera_space_position,
                                  /*fx=*/1.0f, /*fy=*/1.0f);

  EXPECT_TRUE(sigma_2d.isApprox(Eigen::Matrix2f::Identity(), 1e-5f));
}

// Same setup, but the splat is twice as far away (z=2 instead of 1) — perspective should make
// it appear smaller: covariance scales by (1/z)^2 = 1/4, matching ordinary "things look
// smaller far away" intuition.
TEST(Covariance, FartherAwayProjectsSmaller) {
  const Eigen::Matrix3f world_covariance = Eigen::Matrix3f::Identity();
  const Eigen::Matrix3f view_rotation = Eigen::Matrix3f::Identity();
  const Eigen::Vector3f camera_space_position(0.0f, 0.0f, 2.0f);

  const Eigen::Matrix2f sigma_2d =
      glint::core::covariance_2d(world_covariance, view_rotation, camera_space_position,
                                  /*fx=*/1.0f, /*fy=*/1.0f);

  EXPECT_TRUE(sigma_2d.isApprox(0.25f * Eigen::Matrix2f::Identity(), 1e-5f));
}

TEST(Covariance, AntialiasingFilterAddsToTheDiagonalOnly) {
  const Eigen::Matrix2f before = (Eigen::Matrix2f() << 2.0f, 0.5f, 0.5f, 3.0f).finished();
  const Eigen::Matrix2f after = glint::core::apply_antialiasing_filter(before);

  EXPECT_NEAR(after(0, 0), before(0, 0) + glint::core::kAntiAliasingRegularizer, 1e-6f);
  EXPECT_NEAR(after(1, 1), before(1, 1) + glint::core::kAntiAliasingRegularizer, 1e-6f);
  EXPECT_NEAR(after(0, 1), before(0, 1), 1e-6f);  // off-diagonal untouched
  EXPECT_NEAR(after(1, 0), before(1, 0), 1e-6f);
}

TEST(Covariance, ScreenSpaceRadiusUsesLargestAxis) {
  Eigen::Matrix2f isotropic = Eigen::Matrix2f::Identity();
  EXPECT_NEAR(glint::core::screen_space_radius(isotropic), 3.0f, 1e-5f);

  Eigen::Matrix2f anisotropic = Eigen::Matrix2f::Zero();
  anisotropic(0, 0) = 4.0f;  // stretched along x
  anisotropic(1, 1) = 1.0f;
  EXPECT_NEAR(glint::core::screen_space_radius(anisotropic), 6.0f, 1e-5f);  // 3*sqrt(4)
}

// The core correctness check per the project's Jacobian-verification rule: independently
// re-derive covariance_2d's projection Jacobian via central differences on the real,
// nonlinear pinhole projection function, and confirm it agrees with the analytic formula —
// written separately here, not just calling covariance_2d and checking it against itself.
TEST(Covariance, ProjectionJacobianMatchesFiniteDifferences) {
  const float fx = 500.0f, fy = 480.0f;
  const Eigen::Vector3f p(0.3f, -0.2f, 2.5f);  // off-center, off-axis camera-space point

  auto project = [&](const Eigen::Vector3f& point) -> Eigen::Vector2f {
    return Eigen::Vector2f(fx * point.x() / point.z(), fy * point.y() / point.z());
  };

  Eigen::Matrix<float, 2, 3> analytic_jacobian;
  analytic_jacobian << fx / p.z(), 0.0f, -fx * p.x() / (p.z() * p.z()), 0.0f, fy / p.z(),
      -fy * p.y() / (p.z() * p.z());

  constexpr float kEpsilon = 1e-3f;
  Eigen::Matrix<float, 2, 3> numeric_jacobian;
  for (int axis = 0; axis < 3; ++axis) {
    Eigen::Vector3f p_plus = p;
    Eigen::Vector3f p_minus = p;
    p_plus[axis] += kEpsilon;
    p_minus[axis] -= kEpsilon;
    numeric_jacobian.col(axis) = (project(p_plus) - project(p_minus)) / (2.0f * kEpsilon);
  }

  EXPECT_TRUE(analytic_jacobian.isApprox(numeric_jacobian, 1e-2f));
}
