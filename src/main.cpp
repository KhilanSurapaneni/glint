#include <GLFW/glfw3.h>

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "gpu/device.hpp"
#include "viewer/metal_layer_bridge.hpp"

#include <cstdio>

int main() {
  if (!glfwInit()) {
    std::fprintf(stderr, "failed to initialize GLFW\n");
    return 1;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(1280, 720, "glint", nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "failed to create window\n");
    glfwTerminate();
    return 1;
  }

  glint::gpu::Device device;
  CA::MetalLayer* layer = glint::viewer::attach_metal_layer(window, device.device());

  int width = 0, height = 0;
  glfwGetFramebufferSize(window, &width, &height);
  layer->setDrawableSize(CGSize{static_cast<CGFloat>(width), static_cast<CGFloat>(height)});

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    CA::MetalDrawable* drawable = layer->nextDrawable();
    if (!drawable) {
      continue;
    }

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    pass->colorAttachments()->object(0)->setTexture(drawable->texture());
    pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0.05, 0.05, 0.1, 1.0));
    pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

    MTL::CommandBuffer* command_buffer = device.queue()->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass);
    encoder->endEncoding();

    command_buffer->presentDrawable(drawable);
    command_buffer->commit();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
