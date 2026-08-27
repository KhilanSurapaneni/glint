#include <GLFW/glfw3.h>

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <simd/simd.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"

#include "gpu/buffer.hpp"
#include "gpu/device.hpp"
#include "gpu/kernel.hpp"
#include "io/capture_bundle.hpp"
#include "io/dataset.hpp"
#include "io/ios_stream.hpp"
#include "shaders/shared_types.h"
#include "viewer/imgui_metal_bridge.hpp"
#include "viewer/metal_layer_bridge.hpp"
#include "viewer/orbit_camera.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

// GLFW callbacks are plain C function pointers — they can't capture surrounding variables, so
// the camera is retrieved via the window's user pointer instead (set once, in main(), right
// after the camera is constructed).
void scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
  auto* camera = static_cast<glint::viewer::OrbitCamera*>(glfwGetWindowUserPointer(window));
  constexpr float kZoomSensitivity = 0.5f;
  camera->zoom(static_cast<float>(-yoffset) * kZoomSensitivity);  // scroll up = zoom in
}

}  // namespace

int main(int argc, char** argv) {
  // --- Argument parsing: three mutually exclusive point sources ---
  // Default: the fixed Replica dataset (M1's required, public-dataset exit bar). --live
  // listens for the iOS capture app in real time (see docs/CAPTURE_FORMAT.md). --capture
  // loads a previously-saved .glcb file (the app's "Save to File" mode). Both --live and
  // --capture are the stretch track from CLAUDE.md §12 — genuinely useful once there's
  // physical phone access, never required to run the viewer at all.
  enum class Source { kReplicaDataset, kLive, kCaptureFile };
  Source source = Source::kReplicaDataset;
  uint16_t live_port = 5555;
  std::string capture_path;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--live") {
      source = Source::kLive;
    } else if (arg == "--port" && i + 1 < argc) {
      live_port = static_cast<uint16_t>(std::stoi(argv[++i]));
    } else if (arg == "--capture" && i + 1 < argc) {
      source = Source::kCaptureFile;
      capture_path = argv[++i];
    }
  }

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

  // live_server owns the background accept/recv thread for --live; it stays alive for the
  // whole program so the render loop can keep draining newly-arrived frames from it below.
  std::unique_ptr<glint::io::IosStreamServer> live_server;
  glint::io::ReplicaScene dataset_scene;   // populated only for the default source
  glint::io::CaptureBundle capture_bundle; // populated only for --capture
  glint::core::Camera camera;
  // Points at whichever of the two above actually got populated, so the upfront-load loop
  // below is written once instead of twice — --live has no upfront frames at all; its frames
  // arrive later, during the render loop.
  const std::vector<glint::core::Frame>* upfront_frames = nullptr;

  if (source == Source::kLive) {
    std::fprintf(stderr, "waiting for the iOS app to connect on port %d...\n", live_port);
    live_server = std::make_unique<glint::io::IosStreamServer>(live_port);
    camera = live_server->wait_for_camera();
    std::fprintf(stderr, "connected: %dx%d\n", camera.width, camera.height);
  } else if (source == Source::kCaptureFile) {
    capture_bundle = glint::io::load_capture_bundle(capture_path);
    camera = capture_bundle.camera;
    upfront_frames = &capture_bundle.frames;
    std::fprintf(stderr, "loaded %zu frames from %s\n", capture_bundle.frames.size(),
                 capture_path.c_str());
  } else {
    // A sequential prefix of the trajectory, not spread across the room: 200 consecutive
    // frames give dense, overlapping coverage of whichever one area the walkthrough starts
    // in, filling in occlusion holes there for the best-looking single-area cloud. Each frame
    // keeps ~24 MiB resident for the life of the program (positions+colors GPU buffers, plus
    // the CPU-side rgb+depth still owned by `dataset_scene`) — 200 frames is ~4.7 GiB, well
    // inside a 36 GB machine.
    dataset_scene = glint::io::load_replica_scene("assets/replica/Replica/room0",
                                                   /*max_frames=*/200);
    camera = dataset_scene.camera;
    upfront_frames = &dataset_scene.frames;
  }

  const size_t pixel_count = static_cast<size_t>(camera.width) * camera.height;

  glint::gpu::Buffer<uint32_t> image_width_buffer(device.device(), 1);
  image_width_buffer.data()[0] = static_cast<uint32_t>(camera.width);

  glint::gpu::Kernel unproject_kernel(device.device(), device.library(), "unproject_pixel");

  // One positions/colors buffer pair per frame — kept alive for the whole program (rendered
  // every frame), unlike the depth/rgb/params buffers below, which only need to live long
  // enough for that one frame's dispatch.
  std::vector<glint::gpu::Buffer<float>> all_positions;
  std::vector<glint::gpu::Buffer<float>> all_colors;

  // Unprojects one frame on the GPU and appends its points to the cloud. Shared by the
  // dataset/--capture upfront load below and --live's per-frame arrival in the render loop,
  // so every source feeds the renderer through the exact same path.
  const auto unproject_and_append = [&](const glint::core::Frame& frame) {
    glint::gpu::Buffer<float> depth_buffer(device.device(), pixel_count);
    std::copy(frame.depth.begin(), frame.depth.end(), depth_buffer.data());

    glint::gpu::Buffer<uint8_t> rgb_buffer(device.device(), pixel_count * 3);
    std::copy(frame.rgb.begin(), frame.rgb.end(), rgb_buffer.data());

    // Intrinsics + this frame's own pose. The pose conversion below is the one Eigen -> simd
    // translation point discussed when shared_types.h was introduced — Eigen stays the
    // representation everywhere else.
    UnprojectParams unproject_params{};
    for (int col = 0; col < 4; ++col) {
      for (int row = 0; row < 4; ++row) {
        unproject_params.camera_to_world.columns[col][row] = frame.pose.camera_to_world(row, col);
      }
    }
    unproject_params.fx = camera.fx;
    unproject_params.fy = camera.fy;
    unproject_params.cx = camera.cx;
    unproject_params.cy = camera.cy;

    glint::gpu::Buffer<UnprojectParams> unproject_params_buffer(device.device(), 1);
    unproject_params_buffer.data()[0] = unproject_params;

    glint::gpu::Buffer<float> positions_buffer(device.device(), pixel_count * 3);
    glint::gpu::Buffer<float> colors_buffer(device.device(), pixel_count * 3);

    glint::gpu::dispatch_and_wait(
        device.queue(), unproject_kernel,
        {depth_buffer.handle(), rgb_buffer.handle(), unproject_params_buffer.handle(),
         image_width_buffer.handle(), positions_buffer.handle(), colors_buffer.handle()},
        pixel_count);

    all_positions.push_back(std::move(positions_buffer));
    all_colors.push_back(std::move(colors_buffer));
  };

  if (upfront_frames != nullptr) {
    all_positions.reserve(upfront_frames->size());
    all_colors.reserve(upfront_frames->size());
    for (const glint::core::Frame& frame : *upfront_frames) {
      unproject_and_append(frame);
    }
  }

  // Build the point cloud's render pipeline once, up front, and reuse it every frame.
  NS::SharedPtr<MTL::Function> point_vertex_fn = NS::TransferPtr(
      device.library()->newFunction(NS::String::string("point_vertex", NS::UTF8StringEncoding)));
  NS::SharedPtr<MTL::Function> point_fragment_fn = NS::TransferPtr(device.library()->newFunction(
      NS::String::string("point_fragment", NS::UTF8StringEncoding)));

  NS::SharedPtr<MTL::RenderPipelineDescriptor> point_pipeline_desc =
      NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
  point_pipeline_desc->setVertexFunction(point_vertex_fn.get());
  point_pipeline_desc->setFragmentFunction(point_fragment_fn.get());
  point_pipeline_desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  // Without a depth attachment, points draw in whatever order the draw calls happen to run,
  // with no regard for which is actually closer to the camera — from outside looking back
  // through the room, far and near points overlap with zero occlusion. This declares the
  // format; the actual texture/test state are set up below.
  point_pipeline_desc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

  NS::Error* error = nullptr;
  NS::SharedPtr<MTL::RenderPipelineState> point_pipeline_state = NS::TransferPtr(
      device.device()->newRenderPipelineState(point_pipeline_desc.get(), &error));
  if (!point_pipeline_state) {
    std::fprintf(stderr, "failed to create point pipeline state: %s\n",
                 error->localizedDescription()->utf8String());
    return 1;
  }

  // Standard less-than depth test, write enabled — nearer points win, same as any opaque 3D
  // renderer. Created once, reused every frame, like the pipeline state above it.
  NS::SharedPtr<MTL::DepthStencilDescriptor> depth_stencil_desc =
      NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
  depth_stencil_desc->setDepthCompareFunction(MTL::CompareFunctionLess);
  depth_stencil_desc->setDepthWriteEnabled(true);
  NS::SharedPtr<MTL::DepthStencilState> depth_stencil_state =
      NS::TransferPtr(device.device()->newDepthStencilState(depth_stencil_desc.get()));

  // The actual depth buffer, sized to the framebuffer once at startup — MTLDrawable itself
  // has no depth texture of its own, so the render pass needs a separate one attached.
  // alloc()->init() rather than the texture2DDescriptor(...) convenience constructor,
  // matching every other descriptor in this file — Cocoa's convenience factory methods
  // return an autoreleased object, and this project's ownership convention (NS::TransferPtr
  // over an explicit alloc()->init()) assumes an unambiguous +1 reference instead.
  NS::SharedPtr<MTL::TextureDescriptor> depth_texture_desc =
      NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
  depth_texture_desc->setTextureType(MTL::TextureType2D);
  depth_texture_desc->setPixelFormat(MTL::PixelFormatDepth32Float);
  depth_texture_desc->setWidth(static_cast<NS::UInteger>(width));
  depth_texture_desc->setHeight(static_cast<NS::UInteger>(height));
  depth_texture_desc->setUsage(MTL::TextureUsageRenderTarget);
  depth_texture_desc->setStorageMode(MTL::StorageModePrivate);
  NS::SharedPtr<MTL::Texture> depth_texture =
      NS::TransferPtr(device.device()->newTexture(depth_texture_desc.get()));

  glint::viewer::OrbitCamera orbit_camera;
  // Reused every frame and just overwritten — allocating a GPU buffer inside the render loop
  // would violate the project's "no allocation in the per-frame loop" rule.
  glint::gpu::Buffer<simd::float4x4> view_projection_buffer(device.device(), 1);

  // Lets scroll_callback reach the camera despite GLFW callbacks being plain function pointers.
  glfwSetWindowUserPointer(window, &orbit_camera);
  glfwSetScrollCallback(window, scroll_callback);

  double previous_cursor_x = 0.0, previous_cursor_y = 0.0;
  glfwGetCursorPos(window, &previous_cursor_x, &previous_cursor_y);

  // Coarse app-level fps, printed to stderr once a second — not the per-kernel GPU-timestamp
  // profiling BENCHMARKS.md's rasterizer numbers will need later (that measures individual
  // dispatch time; this measures overall wall-clock pacing of the render loop, which CPU
  // timing is the right tool for). Good enough to report a real number instead of a guess.
  auto fps_window_start = std::chrono::steady_clock::now();
  int frames_this_window = 0;

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

    // --live's frames arrive on a background thread at whatever pace the phone sends them;
    // drain and unproject whatever's new each render frame so the cloud visibly grows while
    // walking around, instead of only appearing once the whole session ends.
    if (live_server) {
      for (const glint::core::Frame& frame : live_server->drain_frames()) {
        unproject_and_append(frame);
      }
    }

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

    pass->depthAttachment()->setTexture(depth_texture.get());
    pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
    pass->depthAttachment()->setClearDepth(1.0);
    pass->depthAttachment()->setStoreAction(MTL::StoreActionDontCare);  // discarded after use

    MTL::CommandBuffer* command_buffer = device.queue()->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass);

    // Build this frame's ImGui UI (CPU-side bookkeeping — no GPU drawing happens yet).
    glint::viewer::imgui_metal_new_frame(pass);
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Left-drag orbits the camera — but only when ImGui isn't already using the mouse (e.g.
    // dragging its own panel), so the two input systems don't fight over the same drag.
    double cursor_x = 0.0, cursor_y = 0.0;
    glfwGetCursorPos(window, &cursor_x, &cursor_y);
    if (!ImGui::GetIO().WantCaptureMouse &&
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      constexpr float kOrbitSensitivity = 0.005f;
      const float delta_yaw = static_cast<float>(cursor_x - previous_cursor_x) * kOrbitSensitivity;
      const float delta_pitch =
          static_cast<float>(cursor_y - previous_cursor_y) * kOrbitSensitivity;
      orbit_camera.orbit(delta_yaw, delta_pitch);
    }
    previous_cursor_x = cursor_x;
    previous_cursor_y = cursor_y;

    ImGui::ShowDemoWindow();  // built-in showcase panel, just to prove this all works
    ImGui::Render();

    // Draw the point cloud first, so ImGui's overlay ends up on top of it.
    const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    view_projection_buffer.data()[0] = orbit_camera.view_projection_matrix(aspect_ratio);

    encoder->setRenderPipelineState(point_pipeline_state.get());
    encoder->setDepthStencilState(depth_stencil_state.get());
    encoder->setVertexBuffer(view_projection_buffer.handle(), 0, 2);
    for (size_t i = 0; i < all_positions.size(); ++i) {
      encoder->setVertexBuffer(all_positions[i].handle(), 0, 0);
      encoder->setVertexBuffer(all_colors[i].handle(), 0, 1);
      encoder->drawPrimitives(MTL::PrimitiveTypePoint, NS::UInteger(0), NS::UInteger(pixel_count));
    }

    // Now actually draw the UI ImGui just built, into the same encoder.
    glint::viewer::imgui_metal_render(ImGui::GetDrawData(), command_buffer, encoder);

    encoder->endEncoding();

    command_buffer->presentDrawable(drawable);  // show it once the GPU finishes
    command_buffer->commit();                   // submit the work; don't wait for it

    ++frames_this_window;
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - fps_window_start;
    if (elapsed.count() >= 1.0) {
      std::fprintf(stderr, "fps: %.1f (%zu points)\n", frames_this_window / elapsed.count(),
                   all_positions.size() * pixel_count);
      frames_this_window = 0;
      fps_window_start = now;
    }
  }

  // --- Shutdown: runs once ---

  glint::viewer::imgui_metal_shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
