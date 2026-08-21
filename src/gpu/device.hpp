#pragma once

#include <Metal/Metal.hpp>

namespace glint::gpu {

// Owns the GPU device handle, its command queue, and the compiled shader library. One Device
// is enough for the whole process — construct it once and pass pointers around.
class Device {
 public:
  Device();

  MTL::Device* device() const { return device_.get(); }
  MTL::CommandQueue* queue() const { return queue_.get(); }
  MTL::Library* library() const { return library_.get(); }

 private:
  NS::SharedPtr<MTL::Device> device_;
  NS::SharedPtr<MTL::CommandQueue> queue_;
  NS::SharedPtr<MTL::Library> library_;
};

}  // namespace glint::gpu
