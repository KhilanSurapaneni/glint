#pragma once

#include "core/types.hpp"

#include <filesystem>
#include <vector>

namespace glint::io {

// Everything loaded from one .glcb capture bundle file — the live-capture counterpart to
// ReplicaScene (dataset.hpp), same shape, different source.
struct CaptureBundle {
  core::Camera camera;
  std::vector<core::Frame> frames;
};

// Loads a complete .glcb file written by the iOS app (file mode) or by
// tools/replica_to_glcb.cpp (synthetic test data) — see docs/CAPTURE_FORMAT.md. The live-
// socket counterpart is ios_stream.hpp's IosStreamServer; both share capture_format.cpp's
// parsing logic and differ only in how bytes are fetched.
CaptureBundle load_capture_bundle(const std::filesystem::path& path);

}  // namespace glint::io
