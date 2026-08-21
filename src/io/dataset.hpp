#pragma once

#include "core/types.hpp"

#include <filesystem>
#include <vector>

namespace glint::io {

// Everything loaded from one Replica scene: the shared camera calibration, and one Frame per
// captured timestep.
struct ReplicaScene {
  glint::core::Camera camera;
  std::vector<glint::core::Frame> frames;
};

// Loads a Replica scene from `scene_dir` (e.g. assets/replica/Replica/room0), expecting the
// results/frame*.jpg + results/depth*.png + traj.txt layout documented in
// tools/fetch_replica.sh.
ReplicaScene load_replica_scene(const std::filesystem::path& scene_dir);

}  // namespace glint::io
