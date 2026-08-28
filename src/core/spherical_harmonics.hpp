#pragma once

#include <Eigen/Core>

#include <array>

namespace glint::core {

// Degree-3 real spherical harmonics: 1 + 3 + 5 + 7 = 16 basis functions, used identically for
// each of the 3 color channels. This is the canonical definition of "16 coeffs x 3 channels" —
// splat::SplatSoA's sh_coeffs sizing references these constants rather than duplicating them.
inline constexpr int kShCoeffsPerChannel = 16;
inline constexpr int kShCoeffs = kShCoeffsPerChannel * 3;  // 48

// Real spherical-harmonics basis normalization constants, degree 0-3 (Ramamoorthi & Hanrahan's
// SH lighting formulation — the same values every 3DGS implementation reuses). Not just
// trusted from that citation: tests/test_spherical_harmonics.cpp independently verifies the
// basis functions built from these constants satisfy the defining mathematical property any
// correct orthonormal basis must have (see that test for what that means and why it's a
// stronger check than comparing against a remembered literature value).
inline constexpr float kShC0 = 0.28209479177387814f;
inline constexpr float kShC1 = 0.4886025119029199f;
inline constexpr float kShC2[5] = {1.0925484305920792f, -1.0925484305920792f,
                                    0.31539156525252005f, -1.0925484305920792f,
                                    0.5462742152960396f};
inline constexpr float kShC3[7] = {-0.5900435899266435f, 2.890611442640554f,
                                    -0.4570457994644658f, 0.3731763325901154f,
                                    -0.4570457994644658f, 1.445305721320277f,
                                    -0.5900435899266435f};

// Evaluates all 16 degree-0..3 real SH basis functions at a unit direction vector. Every
// basis function is a fixed polynomial in the direction's (x, y, z) components — no
// trigonometry needed, (x, y, z) already encodes the direction.
inline std::array<float, kShCoeffsPerChannel> evaluate_sh_basis(const Eigen::Vector3f& direction) {
  const float x = direction.x();
  const float y = direction.y();
  const float z = direction.z();
  const float xx = x * x, yy = y * y, zz = z * z;
  const float xy = x * y, yz = y * z, xz = x * z;

  std::array<float, kShCoeffsPerChannel> b{};
  b[0] = kShC0;

  b[1] = -kShC1 * y;
  b[2] = kShC1 * z;
  b[3] = -kShC1 * x;

  b[4] = kShC2[0] * xy;
  b[5] = kShC2[1] * yz;
  b[6] = kShC2[2] * (2.0f * zz - xx - yy);
  b[7] = kShC2[3] * xz;
  b[8] = kShC2[4] * (xx - yy);

  b[9] = kShC3[0] * y * (3.0f * xx - yy);
  b[10] = kShC3[1] * xy * z;
  b[11] = kShC3[2] * y * (4.0f * zz - xx - yy);
  b[12] = kShC3[3] * z * (2.0f * zz - 3.0f * xx - 3.0f * yy);
  b[13] = kShC3[4] * x * (4.0f * zz - xx - yy);
  b[14] = kShC3[5] * z * (xx - yy);
  b[15] = kShC3[6] * x * (xx - 3.0f * yy);

  return b;
}

// Evaluates one splat's actual RGB color for one specific viewing direction — the unit vector
// from the camera toward the splat. `sh` points at kShCoeffs (48) contiguous floats,
// channel-major (splat::SplatSoA's layout: channel c's 16 coefficients at
// sh[c*16 .. c*16+15]). Color is 0.5 + sum(coefficient * basis) per channel — the inverse of
// how splat::initialize_from_frames seeds degree-0 from a pixel's RGB.
inline Eigen::Vector3f evaluate_sh_color(const float* sh, const Eigen::Vector3f& direction) {
  const std::array<float, kShCoeffsPerChannel> basis = evaluate_sh_basis(direction);

  Eigen::Vector3f color(0.5f, 0.5f, 0.5f);
  for (int channel = 0; channel < 3; ++channel) {
    float sum = 0.0f;
    for (int k = 0; k < kShCoeffsPerChannel; ++k) {
      sum += sh[channel * kShCoeffsPerChannel + k] * basis[k];
    }
    color[channel] += sum;
  }
  return color;
}

}  // namespace glint::core
