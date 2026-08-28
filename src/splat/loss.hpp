#pragma once

#include <vector>

namespace glint::splat {

// Mean absolute difference between two same-sized RGB images (row-major, width*height*3
// floats each) — the simpler half of the loss; penalizes per-pixel color error directly.
float l1_loss(const std::vector<float>& rendered, const std::vector<float>& target);

// Structural similarity between two images, via local Gaussian-weighted windows (Wang et al.
// 2004) — unlike L1, this measures whether local *structure* (edges, texture) matches, not
// just raw color, so it penalizes blurriness in a way L1 alone can't. Returns similarity in
// [-1, 1], where 1 means identical.
float ssim(const std::vector<float>& rendered, const std::vector<float>& target, int width,
           int height);

// D-SSIM = (1 - SSIM) / 2 — a *loss* (0 = identical, grows with dissimilarity), the same
// sense of direction as l1_loss, rather than a raw similarity score.
inline float d_ssim_loss(const std::vector<float>& rendered, const std::vector<float>& target,
                          int width, int height) {
  return (1.0f - ssim(rendered, target, width, height)) / 2.0f;
}

// The actual training loss: mostly L1, a smaller D-SSIM contribution for structural
// sharpness (Kerbl et al. 2023's weighting).
inline constexpr float kL1Weight = 0.8f;
inline constexpr float kDSsimWeight = 0.2f;

inline float combined_loss(const std::vector<float>& rendered, const std::vector<float>& target,
                            int width, int height) {
  return kL1Weight * l1_loss(rendered, target) +
         kDSsimWeight * d_ssim_loss(rendered, target, width, height);
}

}  // namespace glint::splat
