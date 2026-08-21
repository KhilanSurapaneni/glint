#pragma once

#include <Metal/Metal.hpp>

#include <cstddef>

namespace glint::gpu {

// A list of N elements of type T, in memory the GPU can see. Uses unified-memory shared
// storage, so data() is directly CPU-readable/writable with no explicit upload/download step.
template <typename T>
class Buffer {
 public:
  Buffer(MTL::Device* device, size_t count)
      : buffer_(NS::TransferPtr(
            device->newBuffer(count * sizeof(T), MTL::ResourceStorageModeShared))),
        count_(count) {}

  MTL::Buffer* handle() const { return buffer_.get(); }  // raw handle, for GPU dispatch calls
  size_t count() const { return count_; }

  T* data() { return static_cast<T*>(buffer_->contents()); }
  const T* data() const { return static_cast<const T*>(buffer_->contents()); }

 private:
  NS::SharedPtr<MTL::Buffer> buffer_;
  size_t count_;
};

}  // namespace glint::gpu
