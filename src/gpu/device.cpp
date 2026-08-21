// This is the one translation unit in the whole program that defines these — see
// metal_cpp's single-header-library convention. Every other file just includes the headers
// normally.
#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include "gpu/device.hpp"

#include <Foundation/Foundation.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <mach-o/dyld.h>

#include <filesystem>
#include <stdexcept>
#include <vector>

namespace glint::gpu {

namespace {

std::filesystem::path executable_dir() {
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> path(size);
  if (_NSGetExecutablePath(path.data(), &size) != 0) {
    throw std::runtime_error("failed to resolve the running executable's path");
  }
  return std::filesystem::path(path.data()).parent_path();
}

}  // namespace

Device::Device() {
  device_ = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
  if (!device_) {
    throw std::runtime_error("no Metal device found");
  }

  queue_ = NS::TransferPtr(device_->newCommandQueue());
  if (!queue_) {
    throw std::runtime_error("failed to create a Metal command queue");
  }

  // We're a plain CLI executable, not an .app bundle, so newDefaultLibrary() has nothing to
  // search — load default.metallib explicitly, resolved next to the executable (CLAUDE.md §5).
  const std::filesystem::path metallib_path = executable_dir() / "default.metallib";
  NS::Error* error = nullptr;
  library_ = NS::TransferPtr(device_->newLibrary(
      NS::String::string(metallib_path.c_str(), NS::UTF8StringEncoding), &error));
  if (!library_) {
    const char* message = error ? error->localizedDescription()->utf8String() : "unknown error";
    throw std::runtime_error(std::string("failed to load default.metallib: ") + message);
  }
}

}  // namespace glint::gpu
