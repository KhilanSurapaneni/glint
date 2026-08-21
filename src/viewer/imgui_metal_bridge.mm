#import <Metal/Metal.h>

#include "imgui_impl_metal.h"

#include "viewer/imgui_metal_bridge.hpp"

namespace glint::viewer {

void imgui_metal_init(MTL::Device* device) {
  ImGui_ImplMetal_Init((__bridge id<MTLDevice>)(void*)device);
}

void imgui_metal_new_frame(MTL::RenderPassDescriptor* pass) {
  ImGui_ImplMetal_NewFrame((__bridge MTLRenderPassDescriptor*)(void*)pass);
}

void imgui_metal_render(ImDrawData* draw_data, MTL::CommandBuffer* command_buffer,
                         MTL::RenderCommandEncoder* encoder) {
  ImGui_ImplMetal_RenderDrawData(draw_data, (__bridge id<MTLCommandBuffer>)(void*)command_buffer,
                                  (__bridge id<MTLRenderCommandEncoder>)(void*)encoder);
}

void imgui_metal_shutdown() {
  ImGui_ImplMetal_Shutdown();
}

}  // namespace glint::viewer
