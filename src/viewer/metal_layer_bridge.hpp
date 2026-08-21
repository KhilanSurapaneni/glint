#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

struct GLFWwindow;

namespace glint::viewer {

// Attaches a CAMetalLayer to `window`'s native macOS view, bound to `device`. Implemented in
// Objective-C++ (metal_layer_bridge.mm) since AppKit isn't reachable from plain C++ or
// metal-cpp — this is the one place in the project that needs it.
CA::MetalLayer* attach_metal_layer(GLFWwindow* window, MTL::Device* device);

}  // namespace glint::viewer
