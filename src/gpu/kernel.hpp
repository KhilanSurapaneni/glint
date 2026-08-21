#pragma once

#include <Metal/Metal.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace glint::gpu {

// A compiled compute pipeline for one Metal kernel function, ready to dispatch.
class Kernel {
 public:
  Kernel(MTL::Device* device, MTL::Library* library, const std::string& function_name) {
    NS::SharedPtr<MTL::Function> function = NS::TransferPtr(
        library->newFunction(NS::String::string(function_name.c_str(), NS::UTF8StringEncoding)));
    if (!function) {
      throw std::runtime_error("Metal function not found: " + function_name);
    }

    NS::Error* error = nullptr;
    pipeline_ = NS::TransferPtr(device->newComputePipelineState(function.get(), &error));
    if (!pipeline_) {
      const char* message = error ? error->localizedDescription()->utf8String() : "unknown error";
      throw std::runtime_error("failed to create pipeline state for " + function_name + ": " +
                                message);
    }
  }

  MTL::ComputePipelineState* pipeline() const { return pipeline_.get(); }

 private:
  NS::SharedPtr<MTL::ComputePipelineState> pipeline_;
};

// Binds `buffers` to consecutive buffer indices starting at 0, dispatches one GPU thread per
// element of `thread_count`, and blocks until the GPU finishes.
inline void dispatch_and_wait(MTL::CommandQueue* queue, const Kernel& kernel,
                               const std::vector<MTL::Buffer*>& buffers, size_t thread_count) {
  NS::SharedPtr<MTL::CommandBuffer> command_buffer = NS::TransferPtr(queue->commandBuffer());
  NS::SharedPtr<MTL::ComputeCommandEncoder> encoder =
      NS::TransferPtr(command_buffer->computeCommandEncoder());

  encoder->setComputePipelineState(kernel.pipeline());
  for (size_t i = 0; i < buffers.size(); ++i) {
    encoder->setBuffer(buffers[i], 0, i);
  }

  const MTL::Size grid_size = MTL::Size::Make(thread_count, 1, 1);
  NS::UInteger max_threads = kernel.pipeline()->maxTotalThreadsPerThreadgroup();
  if (max_threads > thread_count) {
    max_threads = thread_count;
  }
  const MTL::Size threadgroup_size = MTL::Size::Make(max_threads, 1, 1);

  encoder->dispatchThreads(grid_size, threadgroup_size);
  encoder->endEncoding();

  command_buffer->commit();
  command_buffer->waitUntilCompleted();
}

}  // namespace glint::gpu
