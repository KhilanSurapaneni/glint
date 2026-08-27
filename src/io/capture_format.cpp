// This file both decodes JPEG bytes (STB_IMAGE_IMPLEMENTATION, already turned on once in
// dataset.cpp — including stb_image.h again here without the implementation macro just
// declares the same functions) and encodes them (STB_IMAGE_WRITE_IMPLEMENTATION, turned on
// here for the first time in the project).
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "io/capture_format.hpp"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace glint::io {

namespace {

constexpr char kMagic[4] = {'G', 'L', 'C', 'B'};
constexpr uint32_t kFormatVersion = 1;
constexpr int kJpegQuality = 90;

// stb_image_write's memory-buffer variant hands encoded bytes to a callback instead of a file
// path — needed here since the JPEG bytes get a length prefix inside our own stream, not
// written standalone.
void append_to_vector(void* context, void* data, int size) {
  auto* buffer = static_cast<std::vector<uint8_t>*>(context);
  const auto* bytes = static_cast<uint8_t*>(data);
  buffer->insert(buffer->end(), bytes, bytes + size);
}

}  // namespace

core::Camera read_session_header(const ByteReader& read) {
  char magic[4];
  uint32_t version = 0, width = 0, height = 0;
  float fx = 0, fy = 0, cx = 0, cy = 0;

  if (!read(magic, sizeof(magic)) || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
    throw std::runtime_error("not a .glcb stream (bad or missing magic bytes)");
  }
  if (!read(&version, sizeof(version)) || version != kFormatVersion) {
    throw std::runtime_error("unsupported .glcb version");
  }
  if (!read(&width, sizeof(width)) || !read(&height, sizeof(height)) ||
      !read(&fx, sizeof(fx)) || !read(&fy, sizeof(fy)) || !read(&cx, sizeof(cx)) ||
      !read(&cy, sizeof(cy))) {
    throw std::runtime_error("truncated .glcb session header");
  }

  core::Camera camera;
  camera.width = static_cast<int>(width);
  camera.height = static_cast<int>(height);
  camera.fx = fx;
  camera.fy = fy;
  camera.cx = cx;
  camera.cy = cy;
  return camera;
}

std::optional<core::Frame> read_frame_record(const ByteReader& read, int width, int height) {
  // The pose is the first thing in a record — failing to read even its first byte means
  // there simply isn't another record, the normal way a session ends. Any read failure past
  // this point means a record was started but not finished.
  float pose_values[16];
  if (!read(pose_values, sizeof(pose_values))) {
    return std::nullopt;
  }

  core::Frame frame;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      frame.pose.camera_to_world(row, col) = pose_values[row * 4 + col];
    }
  }

  uint32_t rgb_byte_length = 0;
  if (!read(&rgb_byte_length, sizeof(rgb_byte_length))) {
    throw std::runtime_error("truncated frame record (missing rgb length)");
  }
  std::vector<uint8_t> jpeg_bytes(rgb_byte_length);
  if (rgb_byte_length > 0 && !read(jpeg_bytes.data(), rgb_byte_length)) {
    throw std::runtime_error("truncated frame record (short rgb data)");
  }

  int decoded_width = 0, decoded_height = 0, channels = 0;
  uint8_t* pixels = stbi_load_from_memory(jpeg_bytes.data(), static_cast<int>(rgb_byte_length),
                                           &decoded_width, &decoded_height, &channels, 3);
  if (!pixels) {
    throw std::runtime_error("failed to decode frame JPEG");
  }
  frame.rgb.assign(pixels, pixels + (static_cast<size_t>(decoded_width) * decoded_height * 3));
  stbi_image_free(pixels);

  uint32_t depth_byte_length = 0;
  if (!read(&depth_byte_length, sizeof(depth_byte_length))) {
    throw std::runtime_error("truncated frame record (missing depth length)");
  }
  const uint32_t expected_depth_length = static_cast<uint32_t>(width) * height * sizeof(float);
  if (depth_byte_length != expected_depth_length) {
    throw std::runtime_error("frame record's depth length doesn't match the session's width/height");
  }
  frame.depth.resize(static_cast<size_t>(width) * height);
  if (!read(frame.depth.data(), depth_byte_length)) {
    throw std::runtime_error("truncated frame record (short depth data)");
  }

  return frame;
}

void write_session_header(const ByteWriter& write, const core::Camera& camera) {
  write(kMagic, sizeof(kMagic));
  write(&kFormatVersion, sizeof(kFormatVersion));
  const uint32_t width = static_cast<uint32_t>(camera.width);
  const uint32_t height = static_cast<uint32_t>(camera.height);
  write(&width, sizeof(width));
  write(&height, sizeof(height));
  write(&camera.fx, sizeof(camera.fx));
  write(&camera.fy, sizeof(camera.fy));
  write(&camera.cx, sizeof(camera.cx));
  write(&camera.cy, sizeof(camera.cy));
}

void write_frame_record(const ByteWriter& write, const core::Frame& frame, int width, int height) {
  // Row-major 4x4 pose — same convention dataset.cpp reads Replica's traj.txt with.
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      const float value = frame.pose.camera_to_world(row, col);
      write(&value, sizeof(value));
    }
  }

  std::vector<uint8_t> jpeg_bytes;
  stbi_write_jpg_to_func(append_to_vector, &jpeg_bytes, width, height, 3, frame.rgb.data(),
                          kJpegQuality);
  const uint32_t rgb_byte_length = static_cast<uint32_t>(jpeg_bytes.size());
  write(&rgb_byte_length, sizeof(rgb_byte_length));
  write(jpeg_bytes.data(), jpeg_bytes.size());

  const uint32_t depth_byte_length = static_cast<uint32_t>(frame.depth.size() * sizeof(float));
  write(&depth_byte_length, sizeof(depth_byte_length));
  write(frame.depth.data(), depth_byte_length);
}

}  // namespace glint::io
