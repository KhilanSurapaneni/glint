#include <gtest/gtest.h>

#include "splat/loss.hpp"

#include <algorithm>
#include <random>
#include <vector>

namespace {

// A small, deterministic "checkerboard-ish" synthetic image — not flat (a flat image would
// trivially satisfy every SSIM property without actually exercising the local
// variance/covariance terms in the formula).
std::vector<float> make_test_image(int width, int height, unsigned seed) {
  std::vector<float> image(static_cast<size_t>(width) * height * 3);
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  for (float& value : image) {
    value = dist(rng);
  }
  return image;
}

std::vector<float> add_noise(const std::vector<float>& image, float amount, unsigned seed) {
  std::vector<float> result = image;
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> dist(-amount, amount);
  for (float& value : result) {
    value = std::clamp(value + dist(rng), 0.0f, 1.0f);
  }
  return result;
}

}  // namespace

TEST(Loss, L1IsZeroForIdenticalImages) {
  const std::vector<float> image = make_test_image(32, 32, 1);
  EXPECT_NEAR(glint::splat::l1_loss(image, image), 0.0f, 1e-6f);
}

TEST(Loss, L1MatchesHandComputedValueForAConstantDifference) {
  const std::vector<float> a = {0.2f, 0.5f, 0.8f, 0.1f};
  const std::vector<float> b = {0.3f, 0.4f, 0.6f, 0.5f};
  // |0.2-0.3| + |0.5-0.4| + |0.8-0.6| + |0.1-0.5| = 0.1+0.1+0.2+0.4 = 0.8, mean = 0.2
  EXPECT_NEAR(glint::splat::l1_loss(a, b), 0.2f, 1e-6f);
}

// SSIM is verified via the mathematical properties any correct similarity measure must have —
// there's no external reference implementation in this project to compare exact values
// against, the same reasoning test_spherical_harmonics.cpp's orthonormality check uses instead
// of trusting a remembered literature value.
TEST(Loss, SsimIsOneForIdenticalImages) {
  constexpr int kWidth = 32, kHeight = 32;
  const std::vector<float> image = make_test_image(kWidth, kHeight, 42);
  EXPECT_NEAR(glint::splat::ssim(image, image, kWidth, kHeight), 1.0f, 1e-4f);
}

TEST(Loss, DSsimIsZeroForIdenticalImages) {
  constexpr int kWidth = 32, kHeight = 32;
  const std::vector<float> image = make_test_image(kWidth, kHeight, 7);
  EXPECT_NEAR(glint::splat::d_ssim_loss(image, image, kWidth, kHeight), 0.0f, 1e-4f);
}

TEST(Loss, SsimDecreasesMonotonicallyAsImagesDiverge) {
  constexpr int kWidth = 48, kHeight = 48;
  const std::vector<float> base = make_test_image(kWidth, kHeight, 99);
  const std::vector<float> slightly_different = add_noise(base, 0.05f, 1);
  const std::vector<float> very_different = add_noise(base, 0.5f, 2);

  const float ssim_self = glint::splat::ssim(base, base, kWidth, kHeight);
  const float ssim_slight = glint::splat::ssim(base, slightly_different, kWidth, kHeight);
  const float ssim_large = glint::splat::ssim(base, very_different, kWidth, kHeight);

  EXPECT_GT(ssim_self, ssim_slight);
  EXPECT_GT(ssim_slight, ssim_large);
}

TEST(Loss, CombinedLossWeightsL1AndDSsimAsDocumented) {
  constexpr int kWidth = 32, kHeight = 32;
  const std::vector<float> a = make_test_image(kWidth, kHeight, 3);
  const std::vector<float> b = make_test_image(kWidth, kHeight, 4);

  const float expected = glint::splat::kL1Weight * glint::splat::l1_loss(a, b) +
                          glint::splat::kDSsimWeight *
                              glint::splat::d_ssim_loss(a, b, kWidth, kHeight);
  EXPECT_NEAR(glint::splat::combined_loss(a, b, kWidth, kHeight), expected, 1e-6f);
}
