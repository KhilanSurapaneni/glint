#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

struct GLFWwindow;

namespace glint::viewer {

// Attaches a new CAMetalLayer to `window`'s native macOS view, bound to `device`, and returns
// it. Implemented in metal_layer_bridge.mm (Objective-C++) — attaching a layer to an NSView
// isn't reachable from plain C++ or metal-cpp, since AppKit isn't part of what metal-cpp wraps.
CA::MetalLayer* attach_metal_layer(GLFWwindow* window, MTL::Device* device);

}  // namespace glint::viewer
