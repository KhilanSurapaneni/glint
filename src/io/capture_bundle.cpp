#include "io/capture_bundle.hpp"
#include "io/capture_format.hpp"

#include <fstream>
#include <stdexcept>

namespace glint::io {

CaptureBundle load_capture_bundle(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open " + path.string());
  }

  // ifstream::read leaves gcount() telling us whether it actually got `size` bytes or hit
  // EOF first — the exact "true = complete, false = source exhausted" contract every
  // ByteReader follows (see capture_format.hpp).
  const ByteReader read_exact = [&file](void* dst, size_t size) {
    file.read(static_cast<char*>(dst), static_cast<std::streamsize>(size));
    return static_cast<size_t>(file.gcount()) == size;
  };

  CaptureBundle bundle;
  bundle.camera = read_session_header(read_exact);

  while (std::optional<core::Frame> frame =
             read_frame_record(read_exact, bundle.camera.width, bundle.camera.height)) {
    bundle.frames.push_back(std::move(*frame));
  }

  return bundle;
}

}  // namespace glint::io
