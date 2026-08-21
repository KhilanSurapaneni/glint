#include <gtest/gtest.h>

#include "gpu/buffer.hpp"
#include "gpu/device.hpp"
#include "gpu/kernel.hpp"

TEST(GpuAdd, AddsArraysCorrectly) {
  glint::gpu::Device device;

  constexpr size_t kCount = 1024;
  glint::gpu::Buffer<float> a(device.device(), kCount);
  glint::gpu::Buffer<float> b(device.device(), kCount);
  glint::gpu::Buffer<float> c(device.device(), kCount);

  for (size_t i = 0; i < kCount; ++i) {
    a.data()[i] = static_cast<float>(i);
    b.data()[i] = static_cast<float>(2 * i);
  }

  glint::gpu::Kernel add_kernel(device.device(), device.library(), "add_arrays");
  glint::gpu::dispatch_and_wait(device.queue(), add_kernel,
                                 {a.handle(), b.handle(), c.handle()}, kCount);

  for (size_t i = 0; i < kCount; ++i) {
    EXPECT_FLOAT_EQ(c.data()[i], a.data()[i] + b.data()[i]);
  }
}
