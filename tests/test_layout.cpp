#include <gtest/gtest.h>

#include <simd/simd.h>

#include "gpu/buffer.hpp"
#include "gpu/device.hpp"
#include "gpu/kernel.hpp"
#include "shaders/shared_types.h"

TEST(SharedTypesLayout, UnprojectParamsRoundTrips) {
  glint::gpu::Device device;

  UnprojectParams params{};
  params.camera_to_world = simd::float4x4(
      simd::float4{1.0f, 2.0f, 3.0f, 4.0f}, simd::float4{5.0f, 6.0f, 7.0f, 8.0f},
      simd::float4{9.0f, 10.0f, 11.0f, 12.0f}, simd::float4{13.0f, 14.0f, 15.0f, 16.0f});
  params.fx = 600.0f;
  params.fy = 601.0f;
  params.cx = 599.5f;
  params.cy = 339.5f;

  glint::gpu::Buffer<UnprojectParams> params_buffer(device.device(), 1);
  params_buffer.data()[0] = params;

  glint::gpu::Buffer<float> out_buffer(device.device(), 20);

  glint::gpu::Kernel kernel(device.device(), device.library(), "read_unproject_params");
  glint::gpu::dispatch_and_wait(device.queue(), kernel,
                                 {params_buffer.handle(), out_buffer.handle()}, 1);

  const float* out = out_buffer.data();
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      EXPECT_FLOAT_EQ(out[col * 4 + row], params.camera_to_world.columns[col][row]);
    }
  }
  EXPECT_FLOAT_EQ(out[16], params.fx);
  EXPECT_FLOAT_EQ(out[17], params.fy);
  EXPECT_FLOAT_EQ(out[18], params.cx);
  EXPECT_FLOAT_EQ(out[19], params.cy);
}
