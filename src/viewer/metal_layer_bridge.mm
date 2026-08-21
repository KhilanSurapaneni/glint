#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "viewer/metal_layer_bridge.hpp"

namespace glint::viewer {

CA::MetalLayer* attach_metal_layer(GLFWwindow* window, MTL::Device* device) {
  NSWindow* ns_window = glfwGetCocoaWindow(window);
  NSView* view = ns_window.contentView;

  CAMetalLayer* layer = [CAMetalLayer layer];
  layer.device = (__bridge id<MTLDevice>)(void*)device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

  view.layer = layer;
  view.wantsLayer = YES;

  return (CA::MetalLayer*)(__bridge void*)layer;
}

}  // namespace glint::viewer
