#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <filesystem>
#include <limits>
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
// tools/fetch_replica.sh. `max_frames` caps how many frames get loaded (default: all of
// them) — useful for fast local iteration instead of waiting on a full ~30-100s scene load.
// When capped, this is a sequential prefix (frames 0..max_frames-1), not a sample spread
// across the whole trajectory — dense, overlapping coverage of one area of the room rather
// than sparse coverage of the whole thing.
ReplicaScene load_replica_scene(const std::filesystem::path& scene_dir,
                                 size_t max_frames = std::numeric_limits<size_t>::max());

}  // namespace glint::io
