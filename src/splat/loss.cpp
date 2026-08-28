#include "splat/loss.hpp"

#include <cmath>
#include <cstddef>

namespace glint::splat {

float l1_loss(const std::vector<float>& rendered, const std::vector<float>& target) {
  const size_t n = rendered.size();
  float total = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    total += std::fabs(rendered[i] - target[i]);
  }
  return total / static_cast<float>(n);
}

namespace {

// Wang et al. 2004's standard SSIM window: 11x11, Gaussian-weighted, sigma 1.5. Not a fixed
// literature value that needs re-verifying here the way the SH constants did — SSIM has no
// exact-value property to test against (unlike orthonormality); instead it's verified in
// tests/test_loss.cpp via the properties any correct similarity measure must have: identical
// images score 1, and score strictly decreases as images diverge more.
constexpr int kWindowRadius = 5;  // window size = 2*radius + 1 = 11
constexpr float kWindowSigma = 1.5f;
constexpr int kWindowSize = 2 * kWindowRadius + 1;

std::vector<float> build_gaussian_window() {
  std::vector<float> weights(kWindowSize);
  float sum = 0.0f;
  for (int i = 0; i < kWindowSize; ++i) {
    const float x = static_cast<float>(i - kWindowRadius);
    weights[i] = std::exp(-(x * x) / (2.0f * kWindowSigma * kWindowSigma));
    sum += weights[i];
  }
  for (float& w : weights) {
    w /= sum;
  }
  return weights;
}

}  // namespace

float ssim(const std::vector<float>& rendered, const std::vector<float>& target, int width,
           int height) {
  const std::vector<float> window = build_gaussian_window();

  // SSIM's standard stabilizing constants, assuming a [0,1] dynamic range (L=1) — they keep
  // the formula from dividing by (near) zero on flat, low-variance regions.
  constexpr float kC1 = 0.01f * 0.01f;
  constexpr float kC2 = 0.03f * 0.03f;

  double total_ssim = 0.0;
  long sample_count = 0;

  // Only window positions that fit entirely inside the image — simpler than border padding,
  // and doesn't change *what's* being measured, just how many pixels get a vote.
  for (int y = kWindowRadius; y < height - kWindowRadius; ++y) {
    for (int x = kWindowRadius; x < width - kWindowRadius; ++x) {
      for (int channel = 0; channel < 3; ++channel) {
        float mean_a = 0.0f, mean_b = 0.0f;
        for (int wy = -kWindowRadius; wy <= kWindowRadius; ++wy) {
          for (int wx = -kWindowRadius; wx <= kWindowRadius; ++wx) {
            const float weight = window[wy + kWindowRadius] * window[wx + kWindowRadius];
            const size_t index =
                (static_cast<size_t>(y + wy) * width + static_cast<size_t>(x + wx)) * 3 +
                static_cast<size_t>(channel);
            mean_a += weight * rendered[index];
            mean_b += weight * target[index];
          }
        }

        float var_a = 0.0f, var_b = 0.0f, covar = 0.0f;
        for (int wy = -kWindowRadius; wy <= kWindowRadius; ++wy) {
          for (int wx = -kWindowRadius; wx <= kWindowRadius; ++wx) {
            const float weight = window[wy + kWindowRadius] * window[wx + kWindowRadius];
            const size_t index =
                (static_cast<size_t>(y + wy) * width + static_cast<size_t>(x + wx)) * 3 +
                static_cast<size_t>(channel);
            const float diff_a = rendered[index] - mean_a;
            const float diff_b = target[index] - mean_b;
            var_a += weight * diff_a * diff_a;
            var_b += weight * diff_b * diff_b;
            covar += weight * diff_a * diff_b;
          }
        }

        const float numerator = (2.0f * mean_a * mean_b + kC1) * (2.0f * covar + kC2);
        const float denominator =
            (mean_a * mean_a + mean_b * mean_b + kC1) * (var_a + var_b + kC2);
        total_ssim += static_cast<double>(numerator / denominator);
        ++sample_count;
      }
    }
  }

  if (sample_count == 0) {
    return 1.0f;  // image too small for even one full window -- nothing to compare
  }
  return static_cast<float>(total_ssim / static_cast<double>(sample_count));
}

}  // namespace glint::splat
