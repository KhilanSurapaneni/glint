#include <GLFW/glfw3.h>

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"

#include "gpu/device.hpp"
#include "viewer/imgui_metal_bridge.hpp"
#include "viewer/metal_layer_bridge.hpp"

#include <cstdio>

int main() {
  // --- Setup: runs once ---

  if (!glfwInit()) {
    std::fprintf(stderr, "failed to initialize GLFW\n");
    return 1;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // no OpenGL context; we're using Metal instead
  GLFWwindow* window = glfwCreateWindow(1280, 720, "glint", nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "failed to create window\n");
    glfwTerminate();
    return 1;
  }

  glint::gpu::Device device;
  CA::MetalLayer* layer = glint::viewer::attach_metal_layer(window, device.device());

  // Framebuffer size (real pixels), not window size, so Retina displays render sharp.
  int width = 0, height = 0;
  glfwGetFramebufferSize(window, &width, &height);
  layer->setDrawableSize(CGSize{static_cast<CGFloat>(width), static_cast<CGFloat>(height)});

  // Build the triangle's render pipeline once, up front, and reuse it every frame.
  NS::SharedPtr<MTL::Function> vertex_fn = NS::TransferPtr(
      device.library()->newFunction(NS::String::string("triangle_vertex", NS::UTF8StringEncoding)));
  NS::SharedPtr<MTL::Function> fragment_fn = NS::TransferPtr(device.library()->newFunction(
      NS::String::string("triangle_fragment", NS::UTF8StringEncoding)));

  NS::SharedPtr<MTL::RenderPipelineDescriptor> pipeline_desc =
      NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
  pipeline_desc->setVertexFunction(vertex_fn.get());
  pipeline_desc->setFragmentFunction(fragment_fn.get());
  pipeline_desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

  NS::Error* error = nullptr;
  NS::SharedPtr<MTL::RenderPipelineState> pipeline_state =
      NS::TransferPtr(device.device()->newRenderPipelineState(pipeline_desc.get(), &error));
  if (!pipeline_state) {
    std::fprintf(stderr, "failed to create render pipeline state: %s\n",
                 error->localizedDescription()->utf8String());
    return 1;
  }

  // Set up ImGui once: one context, one input backend (GLFW), one render backend (our Metal
  // bridge). Context/input-backend calls are plain C++; the Metal backend goes through the
  // bridge (see imgui_metal_bridge.hpp for why).
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOther(window, true);  // "Other" = not OpenGL/Vulkan, we handle rendering
  glint::viewer::imgui_metal_init(device.device());

  // --- Render loop: runs once per frame, until the window closes ---

  while (!glfwWindowShouldClose(window)) {
    // Drains every temporary GPU/Objective-C object created this frame when it goes out of
    // scope below — without it, they'd never get freed.
    NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

    glfwPollEvents();

    CA::MetalDrawable* drawable = layer->nextDrawable();  // the texture we'll draw into
    if (!drawable) {
      continue;
    }

    // Describe this frame: clear to a dark navy color, then keep whatever gets drawn.
    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    pass->colorAttachments()->object(0)->setTexture(drawable->texture());
    pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0.05, 0.05, 0.1, 1.0));
    pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

    MTL::CommandBuffer* command_buffer = device.queue()->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass);

    // Build this frame's ImGui UI (CPU-side bookkeeping — no GPU drawing happens yet).
    glint::viewer::imgui_metal_new_frame(pass);
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();  // built-in showcase panel, just to prove this all works
    ImGui::Render();

    // Draw our triangle first, so ImGui's overlay ends up on top of it.
    encoder->setRenderPipelineState(pipeline_state.get());
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

    // Now actually draw the UI ImGui just built, into the same encoder.
    glint::viewer::imgui_metal_render(ImGui::GetDrawData(), command_buffer, encoder);

    encoder->endEncoding();

    command_buffer->presentDrawable(drawable);  // show it once the GPU finishes
    command_buffer->commit();                   // submit the work; don't wait for it
  }

  // --- Shutdown: runs once ---

  glint::viewer::imgui_metal_shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
