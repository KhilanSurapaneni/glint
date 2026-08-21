#pragma once

#include <Metal/Metal.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace glint::gpu {

// A compiled, ready-to-dispatch GPU compute program (one function from a .metal file).
class Kernel {
 public:
  Kernel(MTL::Device* device, MTL::Library* library, const std::string& function_name) {
    // Look up the named function inside the compiled shader library.
    NS::SharedPtr<MTL::Function> function = NS::TransferPtr(
        library->newFunction(NS::String::string(function_name.c_str(), NS::UTF8StringEncoding)));
    if (!function) {
      throw std::runtime_error("Metal function not found: " + function_name);
    }

    // Compile that function into a ready-to-run pipeline.
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

// Dispatches `kernel` over `thread_count` GPU threads, binding one buffer per shader
// [[buffer(i)]] index in order, and blocks until the GPU finishes.
inline void dispatch_and_wait(MTL::CommandQueue* queue, const Kernel& kernel,
                               const std::vector<MTL::Buffer*>& buffers, size_t thread_count) {
  NS::SharedPtr<MTL::CommandBuffer> command_buffer = NS::TransferPtr(queue->commandBuffer());
  NS::SharedPtr<MTL::ComputeCommandEncoder> encoder =
      NS::TransferPtr(command_buffer->computeCommandEncoder());

  encoder->setComputePipelineState(kernel.pipeline());
  for (size_t i = 0; i < buffers.size(); ++i) {
    encoder->setBuffer(buffers[i], 0, i);
  }

  // One GPU thread per element; let the pipeline pick a sane threadgroup size.
  const MTL::Size grid_size = MTL::Size::Make(thread_count, 1, 1);
  NS::UInteger max_threads = kernel.pipeline()->maxTotalThreadsPerThreadgroup();
  if (max_threads > thread_count) {
    max_threads = thread_count;
  }
  const MTL::Size threadgroup_size = MTL::Size::Make(max_threads, 1, 1);

  encoder->dispatchThreads(grid_size, threadgroup_size);
  encoder->endEncoding();

  command_buffer->commit();
  command_buffer->waitUntilCompleted();  // block: caller needs the result before returning
}

}  // namespace glint::gpu
