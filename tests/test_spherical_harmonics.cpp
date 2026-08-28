#include <gtest/gtest.h>

#include <Eigen/Core>

#include "core/spherical_harmonics.hpp"

#include <array>
#include <cmath>
#include <vector>

namespace {

// Evenly spreads `count` points across a sphere (a Fibonacci/golden-spiral sampling) — simple,
// deterministic, no RNG needed, and accurate enough to numerically integrate low-degree
// spherical harmonics.
std::vector<Eigen::Vector3f> fibonacci_sphere(int count) {
  std::vector<Eigen::Vector3f> points;
  points.reserve(static_cast<size_t>(count));
  const float golden_angle = static_cast<float>(M_PI) * (3.0f - std::sqrt(5.0f));
  for (int i = 0; i < count; ++i) {
    const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
    const float radius = std::sqrt(std::max(0.0f, 1.0f - y * y));
    const float theta = golden_angle * static_cast<float>(i);
    points.emplace_back(radius * std::cos(theta), y, radius * std::sin(theta));
  }
  return points;
}

}  // namespace

// Real spherical harmonics are, by definition, orthonormal over the sphere: integrating the
// product of two *different* basis functions over every direction is 0; integrating one basis
// function against itself is 1. This test doesn't compare against a remembered literature
// value at all — it numerically integrates (via many sample directions) and checks the basis
// implementation actually satisfies that defining property. A transcription error in any of
// the kShC1/kShC2/kShC3 constants would violate this decisively, which is exactly why this is
// a stronger check than "the constants match what I recall from the paper."
TEST(SphericalHarmonics, BasisFunctionsAreOrthonormal) {
  constexpr int kSampleCount = 20000;
  const std::vector<Eigen::Vector3f> directions = fibonacci_sphere(kSampleCount);

  Eigen::Matrix<float, glint::core::kShCoeffsPerChannel, glint::core::kShCoeffsPerChannel>
      inner_products = decltype(inner_products)::Zero();

  for (const Eigen::Vector3f& direction : directions) {
    const std::array<float, glint::core::kShCoeffsPerChannel> basis =
        glint::core::evaluate_sh_basis(direction);
    for (int i = 0; i < glint::core::kShCoeffsPerChannel; ++i) {
      for (int j = 0; j < glint::core::kShCoeffsPerChannel; ++j) {
        inner_products(i, j) += basis[i] * basis[j];
      }
    }
  }

  // Equal-weight quadrature over the sphere: total solid angle (4*pi) divided evenly among
  // all samples.
  const float solid_angle_per_sample = 4.0f * static_cast<float>(M_PI) / kSampleCount;
  inner_products *= solid_angle_per_sample;

  for (int i = 0; i < glint::core::kShCoeffsPerChannel; ++i) {
    for (int j = 0; j < glint::core::kShCoeffsPerChannel; ++j) {
      const float expected = (i == j) ? 1.0f : 0.0f;
      EXPECT_NEAR(inner_products(i, j), expected, 0.02f) << "basis " << i << " vs " << j;
    }
  }
}

// Degree 0 is a constant (no x/y/z dependence at all) — with every higher-degree coefficient
// left at zero, color must come out identical regardless of viewing direction.
TEST(SphericalHarmonics, DegreeZeroOnlyGivesFlatColorRegardlessOfDirection) {
  float sh[glint::core::kShCoeffs] = {};
  sh[0 * glint::core::kShCoeffsPerChannel] = 1.0f;   // R
  sh[1 * glint::core::kShCoeffsPerChannel] = -0.5f;  // G
  sh[2 * glint::core::kShCoeffsPerChannel] = 2.0f;   // B

  const Eigen::Vector3f color_a = glint::core::evaluate_sh_color(sh, Eigen::Vector3f(1, 0, 0));
  const Eigen::Vector3f color_b =
      glint::core::evaluate_sh_color(sh, Eigen::Vector3f(0, 0, -1).normalized());

  EXPECT_TRUE(color_a.isApprox(color_b, 1e-6f));
  EXPECT_NEAR(color_a.x(), 0.5f + glint::core::kShC0 * 1.0f, 1e-5f);
  EXPECT_NEAR(color_a.y(), 0.5f + glint::core::kShC0 * -0.5f, 1e-5f);
  EXPECT_NEAR(color_a.z(), 0.5f + glint::core::kShC0 * 2.0f, 1e-5f);
}

// Ties evaluate_sh_color directly to splat::initialize_from_frames's init formula: seed
// degree-0 from a color the same way initialization does, evaluate from an arbitrary
// direction, and confirm the original color comes back out exactly.
TEST(SphericalHarmonics, RoundTripsWithInitializationFormula) {
  const Eigen::Vector3f original_color(0.8f, 0.2f, 0.6f);
  float sh[glint::core::kShCoeffs] = {};
  for (int c = 0; c < 3; ++c) {
    sh[c * glint::core::kShCoeffsPerChannel] = (original_color[c] - 0.5f) / glint::core::kShC0;
  }

  const Eigen::Vector3f reconstructed = glint::core::evaluate_sh_color(
      sh, Eigen::Vector3f(0.3f, 0.6f, 0.74f).normalized());

  EXPECT_TRUE(reconstructed.isApprox(original_color, 1e-5f));
}
