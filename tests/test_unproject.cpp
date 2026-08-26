#include <gtest/gtest.h>

#include <simd/simd.h>

#include "gpu/buffer.hpp"
#include "gpu/device.hpp"
#include "gpu/kernel.hpp"
#include "shaders/shared_types.h"

#include <cstdint>

TEST(Unproject, PinholeModelMatchesHandComputedValues) {
  glint::gpu::Device device;

  UnprojectParams params{};
  params.camera_to_world = matrix_identity_float4x4;  // isolates the pinhole formula from the pose multiply
  params.fx = 100.0f;
  params.fy = 100.0f;
  params.cx = 0.0f;
  params.cy = 0.0f;

  constexpr uint32_t kWidth = 2;
  constexpr size_t kCount = 4;  // 2x2

  glint::gpu::Buffer<float> depth(device.device(), kCount);
  depth.data()[0] = 1.0f;
  depth.data()[1] = 2.0f;
  depth.data()[2] = 3.0f;
  depth.data()[3] = 4.0f;

  glint::gpu::Buffer<uint8_t> rgb(device.device(), kCount * 3);
  for (size_t i = 0; i < kCount * 3; ++i) {
    rgb.data()[i] = static_cast<uint8_t>(i * 10);
  }

  glint::gpu::Buffer<UnprojectParams> params_buffer(device.device(), 1);
  params_buffer.data()[0] = params;

  glint::gpu::Buffer<uint32_t> width_buffer(device.device(), 1);
  width_buffer.data()[0] = kWidth;

  glint::gpu::Buffer<float> positions(device.device(), kCount * 3);
  glint::gpu::Buffer<float> colors(device.device(), kCount * 3);

  glint::gpu::Kernel kernel(device.device(), device.library(), "unproject_pixel");
  glint::gpu::dispatch_and_wait(
      device.queue(), kernel,
      {depth.handle(), rgb.handle(), params_buffer.handle(), width_buffer.handle(),
       positions.handle(), colors.handle()},
      kCount);

  // Hand-computed on paper, independently of the kernel's own code: fx=fy=100, cx=cy=0,
  // identity pose (camera space == world space here), depths 1/2/3/4 at pixels (0,0), (1,0),
  // (0,1), (1,1).
  const float* p = positions.data();
  EXPECT_NEAR(p[0 * 3 + 0], 0.00f, 1e-5f);
  EXPECT_NEAR(p[0 * 3 + 1], 0.00f, 1e-5f);
  EXPECT_NEAR(p[0 * 3 + 2], 1.00f, 1e-5f);

  EXPECT_NEAR(p[1 * 3 + 0], 0.02f, 1e-5f);
  EXPECT_NEAR(p[1 * 3 + 1], 0.00f, 1e-5f);
  EXPECT_NEAR(p[1 * 3 + 2], 2.00f, 1e-5f);

  EXPECT_NEAR(p[2 * 3 + 0], 0.00f, 1e-5f);
  EXPECT_NEAR(p[2 * 3 + 1], 0.03f, 1e-5f);
  EXPECT_NEAR(p[2 * 3 + 2], 3.00f, 1e-5f);

  EXPECT_NEAR(p[3 * 3 + 0], 0.04f, 1e-5f);
  EXPECT_NEAR(p[3 * 3 + 1], 0.04f, 1e-5f);
  EXPECT_NEAR(p[3 * 3 + 2], 4.00f, 1e-5f);
}
