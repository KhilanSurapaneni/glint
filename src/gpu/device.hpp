#pragma once

#include <Metal/Metal.hpp>

namespace glint::gpu {

// Owns the GPU connection: the device handle, a command queue to submit work through, and the
// compiled shader library. Construct one Device near program start and share pointers from it.
class Device {
 public:
  Device();  // does the actual GPU handshake; throws on failure

  MTL::Device* device() const { return device_.get(); }
  MTL::CommandQueue* queue() const { return queue_.get(); }
  MTL::Library* library() const { return library_.get(); }

 private:
  NS::SharedPtr<MTL::Device> device_;
  NS::SharedPtr<MTL::CommandQueue> queue_;
  NS::SharedPtr<MTL::Library> library_;
};

}  // namespace glint::gpu
