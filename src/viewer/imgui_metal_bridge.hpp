#pragma once

#include <Metal/Metal.hpp>

#include "imgui.h"

namespace glint::viewer {

// Dear ImGui's Metal backend (imgui_impl_metal.h) declares its functions using Apple's native
// Objective-C Metal types (id<MTLDevice>, etc.), which plain C++ can't even parse — so nothing
// in main.cpp can include that header directly. These bridge the 4 calls our render loop
// needs, so the rest of the project can stay plain C++.
void imgui_metal_init(MTL::Device* device);
void imgui_metal_new_frame(MTL::RenderPassDescriptor* pass);
void imgui_metal_render(ImDrawData* draw_data, MTL::CommandBuffer* command_buffer,
                         MTL::RenderCommandEncoder* encoder);
void imgui_metal_shutdown();

}  // namespace glint::viewer
