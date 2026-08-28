#pragma once

#include "shaders/shared_types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace glint::splat {

// One entry in the tile-sorted duplicated key list: which splat this is, and the value it
// actually gets sorted by. Packing (tile_id, depth) into one 64-bit key means a single
// ordinary sort produces exactly the order the rasterizer needs — every splat grouped by
// tile, and within a tile, ordered front-to-back (nearest first).
struct SplatKey {
  uint64_t sort_key;  // (tile_id << 32) | depth_bits
  uint32_t splat_index;
};

// Index range [start, end) into a sorted key list belonging to one tile — start == end means
// the tile has no splats at all.
struct TileRange {
  uint32_t start = 0;
  uint32_t end = 0;
};

struct SortResult {
  std::vector<SplatKey> sorted_keys;
  // One entry per tile, row-major: tile (x, y) lives at tile_ranges[y * tiles_x + x].
  std::vector<TileRange> tile_ranges;
};

// Builds the duplicated key list from Phase 2's per-splat outputs (screen position, depth,
// radius, tile-touch-count — the last of which is 0 for anything Phase 2 already culled),
// sorts it, and identifies each tile's range within the sorted list. Entirely CPU-side — Stage
// A from Open Decision #3 (see docs/M2_PLAN.md Phase 3 for why: unblocks the rest of the
// pipeline immediately, and doubles as the correctness reference the future GPU radix sort
// gets tested against).
SortResult sort_splats_by_tile(const float* screen_positions, const float* depths,
                                const float* radii, const uint32_t* tile_touch_counts,
                                size_t splat_count, uint32_t image_width, uint32_t image_height,
                                uint32_t tile_size = GLINT_TILE_SIZE);

}  // namespace glint::splat
