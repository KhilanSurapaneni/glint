#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "viewer/metal_layer_bridge.hpp"

namespace glint::viewer {

CA::MetalLayer* attach_metal_layer(GLFWwindow* window, MTL::Device* device) {
  NSWindow* ns_window = glfwGetCocoaWindow(window);  // the real macOS window behind GLFW's handle
  NSView* view = ns_window.contentView;

  CAMetalLayer* layer = [CAMetalLayer layer];
  layer.device = (__bridge id<MTLDevice>)(void*)device;  // cross from our C++ pointer to Objective-C
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

  view.layer = layer;  // actually attach the layer to the window
  view.wantsLayer = YES;

  return (CA::MetalLayer*)(__bridge void*)layer;  // hand back a plain C++-usable pointer
}

}  // namespace glint::viewer
