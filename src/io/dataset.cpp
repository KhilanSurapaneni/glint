// The one file in the program that turns on stb_image's actual implementation code — see
// metal-cpp's *_PRIVATE_IMPLEMENTATION macros for the same pattern.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "io/dataset.hpp"

#include <Eigen/Core>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace glint::io {

namespace {

// Replica's fixed camera calibration — same for every scene. See tools/fetch_replica.sh for
// where these numbers were verified against.
constexpr int kWidth = 1200;
constexpr int kHeight = 680;
constexpr float kFx = 600.0f;
constexpr float kFy = 600.0f;
constexpr float kCx = 599.5f;
constexpr float kCy = 339.5f;
constexpr float kDepthScale = 6553.5f;

std::vector<Eigen::Matrix4f> load_poses(const std::filesystem::path& traj_path) {
  std::ifstream file(traj_path);
  if (!file) {
    throw std::runtime_error("failed to open " + traj_path.string());
  }

  std::vector<Eigen::Matrix4f> poses;
  std::string line;
  while (std::getline(file, line)) {
    std::istringstream stream(line);
    Eigen::Matrix4f pose;
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        stream >> pose(row, col);
      }
    }
    // Coordinate-convention fix, confirmed against NICE-SLAM's own loader: the renderer's Y
    // and Z axes are flipped relative to the standard vision/OpenCV camera convention.
    pose.col(1) *= -1.0f;
    pose.col(2) *= -1.0f;
    poses.push_back(pose);
  }
  return poses;
}

std::string zero_padded(int index, int width) {
  std::ostringstream stream;
  stream << std::setfill('0') << std::setw(width) << index;
  return stream.str();
}

glint::core::Frame load_frame(const std::filesystem::path& results_dir, int index,
                               const Eigen::Matrix4f& pose) {
  glint::core::Frame frame;
  frame.pose.camera_to_world = pose;

  const std::string number = zero_padded(index, 6);

  int width = 0, height = 0, channels = 0;
  const std::filesystem::path rgb_path = results_dir / ("frame" + number + ".jpg");
  unsigned char* rgb_pixels = stbi_load(rgb_path.string().c_str(), &width, &height, &channels, 3);
  if (!rgb_pixels) {
    throw std::runtime_error("failed to load " + rgb_path.string());
  }
  frame.rgb.assign(rgb_pixels, rgb_pixels + (width * height * 3));
  stbi_image_free(rgb_pixels);

  const std::filesystem::path depth_path = results_dir / ("depth" + number + ".png");
  unsigned short* depth_raw =
      stbi_load_16(depth_path.string().c_str(), &width, &height, &channels, 1);
  if (!depth_raw) {
    throw std::runtime_error("failed to load " + depth_path.string());
  }
  frame.depth.resize(static_cast<size_t>(width) * height);
  for (size_t i = 0; i < frame.depth.size(); ++i) {
    frame.depth[i] = static_cast<float>(depth_raw[i]) / kDepthScale;  // raw units -> meters
  }
  stbi_image_free(depth_raw);

  return frame;
}

}  // namespace

ReplicaScene load_replica_scene(const std::filesystem::path& scene_dir) {
  ReplicaScene scene;
  scene.camera.width = kWidth;
  scene.camera.height = kHeight;
  scene.camera.fx = kFx;
  scene.camera.fy = kFy;
  scene.camera.cx = kCx;
  scene.camera.cy = kCy;

  const std::vector<Eigen::Matrix4f> poses = load_poses(scene_dir / "traj.txt");

  scene.frames.reserve(poses.size());
  const std::filesystem::path results_dir = scene_dir / "results";
  for (size_t i = 0; i < poses.size(); ++i) {
    scene.frames.push_back(load_frame(results_dir, static_cast<int>(i), poses[i]));
  }

  return scene;
}

}  // namespace glint::io
