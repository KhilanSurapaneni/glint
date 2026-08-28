#include "splat/sort.hpp"

#include <algorithm>
#include <bit>

namespace glint::splat {

namespace {

// Same tile bounding-box formula preprocess.metal uses to decide tile_touch_count —
// recomputed here rather than stored separately, since it's cheap and keeps Phase 2's output
// buffers unchanged.
struct TileBounds {
  int min_x, max_x, min_y, max_y;
};

TileBounds compute_tile_bounds(float screen_x, float screen_y, float radius, uint32_t tiles_x,
                                uint32_t tiles_y, uint32_t tile_size) {
  const int max_tile_index_x = static_cast<int>(tiles_x) - 1;
  const int max_tile_index_y = static_cast<int>(tiles_y) - 1;

  TileBounds bounds;
  bounds.min_x = std::clamp(static_cast<int>((screen_x - radius) / tile_size), 0, max_tile_index_x);
  bounds.max_x = std::clamp(static_cast<int>((screen_x + radius) / tile_size), 0, max_tile_index_x);
  bounds.min_y = std::clamp(static_cast<int>((screen_y - radius) / tile_size), 0, max_tile_index_y);
  bounds.max_y = std::clamp(static_cast<int>((screen_y + radius) / tile_size), 0, max_tile_index_y);
  return bounds;
}

}  // namespace

SortResult sort_splats_by_tile(const float* screen_positions, const float* depths,
                                const float* radii, const uint32_t* tile_touch_counts,
                                size_t splat_count, uint32_t image_width, uint32_t image_height,
                                uint32_t tile_size) {
  const uint32_t tiles_x = (image_width + tile_size - 1) / tile_size;
  const uint32_t tiles_y = (image_height + tile_size - 1) / tile_size;
  const uint32_t tile_count = tiles_x * tiles_y;

  // One entry per (splat, touched-tile) pair — reserved up front so the loop below never
  // reallocates mid-way.
  size_t total_entries = 0;
  for (size_t i = 0; i < splat_count; ++i) {
    total_entries += tile_touch_counts[i];
  }

  std::vector<SplatKey> keys;
  keys.reserve(total_entries);

  for (size_t i = 0; i < splat_count; ++i) {
    if (tile_touch_counts[i] == 0) {
      continue;  // culled or fully off-screen in Phase 2 — nothing to duplicate
    }

    const float screen_x = screen_positions[i * 2 + 0];
    const float screen_y = screen_positions[i * 2 + 1];
    const TileBounds bounds =
        compute_tile_bounds(screen_x, screen_y, radii[i], tiles_x, tiles_y, tile_size);

    // Depth is always positive (Phase 2 already culled anything at/behind the camera), so its
    // raw IEEE-754 bit pattern sorts in the same order as the float value itself — no
    // float-to-sortable-uint transform needed (that trick is only for possibly-negative
    // values).
    const uint32_t depth_bits = std::bit_cast<uint32_t>(depths[i]);

    for (int tile_y = bounds.min_y; tile_y <= bounds.max_y; ++tile_y) {
      for (int tile_x = bounds.min_x; tile_x <= bounds.max_x; ++tile_x) {
        const uint32_t tile_id =
            static_cast<uint32_t>(tile_y) * tiles_x + static_cast<uint32_t>(tile_x);
        const uint64_t sort_key = (static_cast<uint64_t>(tile_id) << 32) | depth_bits;
        keys.push_back(SplatKey{sort_key, static_cast<uint32_t>(i)});
      }
    }
  }

  // The actual sort — Stage A (Open Decision #3). Sorting by sort_key alone is exactly "group
  // by tile, then order front-to-back within a tile" in one step, since tile_id occupies the
  // high 32 bits and depth the low 32 bits of the same key.
  std::sort(keys.begin(), keys.end(),
            [](const SplatKey& a, const SplatKey& b) { return a.sort_key < b.sort_key; });

  // Tile-range identification: one pass over the now-sorted list, recording where each tile's
  // entries begin and end.
  std::vector<TileRange> tile_ranges(tile_count);
  for (size_t i = 0; i < keys.size(); ++i) {
    const uint32_t tile_id = static_cast<uint32_t>(keys[i].sort_key >> 32);
    const bool is_first_entry_for_this_tile =
        (i == 0) || (tile_id != static_cast<uint32_t>(keys[i - 1].sort_key >> 32));
    if (is_first_entry_for_this_tile) {
      tile_ranges[tile_id].start = static_cast<uint32_t>(i);
    }
    tile_ranges[tile_id].end = static_cast<uint32_t>(i + 1);
  }

  return SortResult{std::move(keys), std::move(tile_ranges)};
}

}  // namespace glint::splat
