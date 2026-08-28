#pragma once

#include "core/spherical_harmonics.hpp"
#include "core/types.hpp"
#include "gpu/buffer.hpp"
#include "gpu/device.hpp"

#include <Metal/Metal.hpp>

#include <cmath>
#include <cstddef>
#include <vector>

namespace glint::splat {

// Re-exported from core:: (the canonical definition — core has no dependency on splat, so the
// constant has to live there, not here) purely so existing call sites can keep writing
// splat::kShCoeffs without an extra namespace to remember.
inline constexpr int kShCoeffsPerChannel = core::kShCoeffsPerChannel;
inline constexpr int kShCoeffs = core::kShCoeffs;

// One 3D Gaussian splat's parameters, for `count` splats, stored as structure-of-arrays —
// never array-of-structs (CLAUDE.md §6.1): a GPU kernel with one thread per splat has every
// thread reading the same field at once, and SoA keeps that one coalesced memory access
// instead of a scattered read through unrelated fields.
//
// Two fields are stored in an unconstrained "raw" space rather than their real, physically
// meaningful space, specifically so gradient descent can never push them somewhere invalid:
//   - scale must be positive (no such thing as negative size) -> stored as its natural log;
//     recovered with exp(), which is positive for every real input.
//   - opacity must be in [0, 1] -> stored as its logit; recovered with sigmoid(), which maps
//     every real input into (0, 1).
// The optimizer is free to add/subtract any real number from these raw values every step
// without ever producing a scale that needs clamping or an opacity that needs clipping — the
// activation function makes invalid values structurally impossible, not just discouraged.
struct SplatSoA {
  // All buffers below share this many splats, indexed the same way: splat i's rotation is
  // rotations[i*4 .. i*4+3], its SH coefficients are sh_coeffs[i*48 .. i*48+47], etc. — flat
  // scalar arrays, not nested per-splat structs, per CLAUDE.md §8's "boring beats clever" rule
  // for anything that might eventually cross into a Metal kernel.
  gpu::Buffer<float> positions;        // count * 3 (x, y, z), world space
  gpu::Buffer<float> rotations;        // count * 4 (x, y, z, w quaternion, un-normalized)
  gpu::Buffer<float> log_scales;       // count * 3, natural log of the real (x, y, z) scale
  gpu::Buffer<float> logit_opacities;  // count * 1, logit of the real [0,1] opacity
  // count * 48: for splat i, channel c (0=R, 1=G, 2=B), coefficient k (0..15), the value
  // lives at sh_coeffs[i*48 + c*16 + k] — channel-major, coefficients contiguous within it.
  gpu::Buffer<float> sh_coeffs;

  size_t count;

  SplatSoA(MTL::Device* device, size_t splat_count)
      : positions(device, splat_count * 3),
        rotations(device, splat_count * 4),
        log_scales(device, splat_count * 3),
        logit_opacities(device, splat_count),
        sh_coeffs(device, splat_count * kShCoeffs),
        count(splat_count) {}
};

// --- Activation functions: raw (unconstrained) <-> real (physically meaningful) space ---
// CPU-side reference versions. Metal can't call these directly, so the GPU preprocess kernel
// (Phase 2) implements the same formulas again in MSL, checked against these for agreement.

inline float scale_from_log(float log_scale) { return std::exp(log_scale); }
inline float log_from_scale(float scale) { return std::log(scale); }

inline float opacity_from_logit(float logit) { return 1.0f / (1.0f + std::exp(-logit)); }
inline float logit_from_opacity(float opacity) { return std::log(opacity / (1.0f - opacity)); }

// Un-normalized quaternions drift under gradient descent (nothing keeps their magnitude at 1)
// — every *read* normalizes, rather than every write, so the raw stored value stays whatever
// the optimizer last set it to, and only the interpretation of it is corrected.
inline void normalize_quaternion(float& x, float& y, float& z, float& w) {
  const float norm = std::sqrt(x * x + y * y + z * z + w * w);
  x /= norm;
  y /= norm;
  z /= norm;
  w /= norm;
}

// Builds initial splats from a set of posed RGB-D frames: unprojects every frame's depth
// (M1's kernel, reused), pools the resulting points across all of them, subsamples down to
// roughly `target_splat_count`, and seeds every SplatSoA field from that subsample — position
// direct from the point, identity rotation, scale from that pixel's real-world footprint,
// a fixed modest starting opacity, and degree-0 SH color from the pixel's RGB (higher SH
// degrees start at zero). See docs/M2_PLAN.md Phase 1.3 for the reasoning behind each choice.
// Which frames go in (how many, train vs. held-out) is the caller's decision, not this
// function's — it just builds splats from whatever frames it's given.
SplatSoA initialize_from_frames(gpu::Device& device, const std::vector<core::Frame>& frames,
                                 const core::Camera& camera, size_t target_splat_count);

}  // namespace glint::splat
