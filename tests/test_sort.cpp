#include <gtest/gtest.h>

#include "splat/sort.hpp"

#include <algorithm>
#include <bit>
#include <random>
#include <vector>

namespace {

// A 64x64 image with 16px tiles is a 4x4 tile grid — small enough that which tile a splat
// lands in can be worked out by hand: tile (tx, ty) covers screen pixels
// [tx*16, tx*16+16) x [ty*16, ty*16+16), tile_id = ty*4 + tx.
constexpr uint32_t kImageWidth = 64;
constexpr uint32_t kImageHeight = 64;
constexpr uint32_t kTileSize = 16;

}  // namespace

TEST(Sort, NoSplatsProducesEmptyResult) {
  const glint::splat::SortResult result =
      glint::splat::sort_splats_by_tile(nullptr, nullptr, nullptr, nullptr, 0, kImageWidth,
                                         kImageHeight, kTileSize);

  EXPECT_TRUE(result.sorted_keys.empty());
  ASSERT_EQ(result.tile_ranges.size(), 16u);  // 4x4 grid
  for (const glint::splat::TileRange& range : result.tile_ranges) {
    EXPECT_EQ(range.start, 0u);
    EXPECT_EQ(range.end, 0u);
  }
}

TEST(Sort, CulledSplatsAreSkipped) {
  const float screen_positions[2] = {8.0f, 8.0f};
  const float depths[1] = {5.0f};
  const float radii[1] = {1.0f};
  const uint32_t tile_touch_counts[1] = {0};  // Phase 2 already culled this one

  const glint::splat::SortResult result = glint::splat::sort_splats_by_tile(
      screen_positions, depths, radii, tile_touch_counts, 1, kImageWidth, kImageHeight, kTileSize);

  EXPECT_TRUE(result.sorted_keys.empty());
}

TEST(Sort, SingleSplatLandsInExactlyOneTile) {
  // Centered in tile (0, 0), small radius -- stays within that one tile.
  const float screen_positions[2] = {8.0f, 8.0f};
  const float depths[1] = {3.0f};
  const float radii[1] = {1.0f};
  const uint32_t tile_touch_counts[1] = {1};

  const glint::splat::SortResult result = glint::splat::sort_splats_by_tile(
      screen_positions, depths, radii, tile_touch_counts, 1, kImageWidth, kImageHeight, kTileSize);

  ASSERT_EQ(result.sorted_keys.size(), 1u);
  EXPECT_EQ(result.sorted_keys[0].splat_index, 0u);

  constexpr uint32_t kTileId = 0;  // tile (0, 0)
  EXPECT_EQ(result.tile_ranges[kTileId].start, 0u);
  EXPECT_EQ(result.tile_ranges[kTileId].end, 1u);
  // Every other tile stays empty.
  for (uint32_t tile_id = 0; tile_id < result.tile_ranges.size(); ++tile_id) {
    if (tile_id != kTileId) {
      EXPECT_EQ(result.tile_ranges[tile_id].start, result.tile_ranges[tile_id].end);
    }
  }
}

TEST(Sort, SplatsInDifferentTilesDontMix) {
  // Splat 0 in tile (0, 0); splat 1 in tile (2, 2) -- far enough apart that neither's small
  // radius reaches the other's tile.
  const float screen_positions[4] = {8.0f, 8.0f, 40.0f, 40.0f};
  const float depths[2] = {5.0f, 5.0f};
  const float radii[2] = {1.0f, 1.0f};
  const uint32_t tile_touch_counts[2] = {1, 1};

  const glint::splat::SortResult result = glint::splat::sort_splats_by_tile(
      screen_positions, depths, radii, tile_touch_counts, 2, kImageWidth, kImageHeight, kTileSize);

  ASSERT_EQ(result.sorted_keys.size(), 2u);

  constexpr uint32_t kTileA = 0;       // (0, 0)
  constexpr uint32_t kTileB = 2 * 4 + 2;  // (2, 2) -> ty*4+tx = 10

  ASSERT_EQ(result.tile_ranges[kTileA].end - result.tile_ranges[kTileA].start, 1u);
  ASSERT_EQ(result.tile_ranges[kTileB].end - result.tile_ranges[kTileB].start, 1u);
  EXPECT_EQ(result.sorted_keys[result.tile_ranges[kTileA].start].splat_index, 0u);
  EXPECT_EQ(result.sorted_keys[result.tile_ranges[kTileB].start].splat_index, 1u);
}

TEST(Sort, SameTileSortsNearestFirst) {
  // Both splats land in tile (0, 0). Splat 0 is farther (depth 5), splat 1 is nearer (depth
  // 2) -- nearest must come first in the sorted order (front-to-back).
  const float screen_positions[4] = {8.0f, 8.0f, 8.0f, 8.0f};
  const float depths[2] = {5.0f, 2.0f};
  const float radii[2] = {1.0f, 1.0f};
  const uint32_t tile_touch_counts[2] = {1, 1};

  const glint::splat::SortResult result = glint::splat::sort_splats_by_tile(
      screen_positions, depths, radii, tile_touch_counts, 2, kImageWidth, kImageHeight, kTileSize);

  constexpr uint32_t kTileId = 0;
  ASSERT_EQ(result.tile_ranges[kTileId].end - result.tile_ranges[kTileId].start, 2u);
  const uint32_t first = result.tile_ranges[kTileId].start;
  EXPECT_EQ(result.sorted_keys[first].splat_index, 1u);      // depth 2, nearer, comes first
  EXPECT_EQ(result.sorted_keys[first + 1].splat_index, 0u);  // depth 5, farther, comes second
}

TEST(Sort, SplatSpanningFourTilesProducesFourEntries) {
  // Centered right at the corner shared by tiles (0,0), (1,0), (0,1), (1,1), with a radius
  // wide enough to reach into all four.
  const float screen_positions[2] = {16.0f, 16.0f};
  const float depths[1] = {4.0f};
  const float radii[1] = {10.0f};
  const uint32_t tile_touch_counts[1] = {4};

  const glint::splat::SortResult result = glint::splat::sort_splats_by_tile(
      screen_positions, depths, radii, tile_touch_counts, 1, kImageWidth, kImageHeight, kTileSize);

  ASSERT_EQ(result.sorted_keys.size(), 4u);

  const uint32_t expected_tiles[4] = {0 * 4 + 0, 0 * 4 + 1, 1 * 4 + 0, 1 * 4 + 1};
  for (uint32_t tile_id : expected_tiles) {
    ASSERT_EQ(result.tile_ranges[tile_id].end - result.tile_ranges[tile_id].start, 1u)
        << "tile " << tile_id;
    EXPECT_EQ(result.sorted_keys[result.tile_ranges[tile_id].start].splat_index, 0u);
  }
}

// A larger, randomized case: independently rebuild the expected duplicated-key list with a
// separate, simpler loop (not reusing sort_splats_by_tile's own tile-bounds logic in spirit —
// this duplicates the "flatten the bounding box" step by hand) and confirm the function's
// output matches, both in total count and in per-tile sorted order.
TEST(Sort, LargerRandomizedCaseMatchesIndependentReference) {
  constexpr size_t kSplatCount = 500;
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> position_dist(0.0f, 64.0f);
  std::uniform_real_distribution<float> depth_dist(0.1f, 100.0f);
  std::uniform_real_distribution<float> radius_dist(0.5f, 6.0f);

  std::vector<float> screen_positions(kSplatCount * 2);
  std::vector<float> depths(kSplatCount);
  std::vector<float> radii(kSplatCount);
  std::vector<uint32_t> tile_touch_counts(kSplatCount);

  const uint32_t tiles_x = kImageWidth / kTileSize;
  const uint32_t tiles_y = kImageHeight / kTileSize;

  // Independent reference: same bounding-box idea, written separately here rather than
  // calling into splat::sort's internals.
  std::vector<glint::splat::SplatKey> expected_keys;

  for (size_t i = 0; i < kSplatCount; ++i) {
    const float x = position_dist(rng);
    const float y = position_dist(rng);
    const float depth = depth_dist(rng);
    const float radius = radius_dist(rng);

    screen_positions[i * 2 + 0] = x;
    screen_positions[i * 2 + 1] = y;
    depths[i] = depth;
    radii[i] = radius;

    const int min_tx = std::clamp(static_cast<int>((x - radius) / kTileSize), 0,
                                   static_cast<int>(tiles_x) - 1);
    const int max_tx = std::clamp(static_cast<int>((x + radius) / kTileSize), 0,
                                   static_cast<int>(tiles_x) - 1);
    const int min_ty = std::clamp(static_cast<int>((y - radius) / kTileSize), 0,
                                   static_cast<int>(tiles_y) - 1);
    const int max_ty = std::clamp(static_cast<int>((y + radius) / kTileSize), 0,
                                   static_cast<int>(tiles_y) - 1);

    uint32_t count = 0;
    for (int ty = min_ty; ty <= max_ty; ++ty) {
      for (int tx = min_tx; tx <= max_tx; ++tx) {
        const uint32_t tile_id = static_cast<uint32_t>(ty) * tiles_x + static_cast<uint32_t>(tx);
        const uint32_t depth_bits = std::bit_cast<uint32_t>(depth);
        const uint64_t key = (static_cast<uint64_t>(tile_id) << 32) | depth_bits;
        expected_keys.push_back(glint::splat::SplatKey{key, static_cast<uint32_t>(i)});
        ++count;
      }
    }
    tile_touch_counts[i] = count;
  }
  std::sort(expected_keys.begin(), expected_keys.end(),
            [](const glint::splat::SplatKey& a, const glint::splat::SplatKey& b) {
              return a.sort_key < b.sort_key;
            });

  const glint::splat::SortResult result = glint::splat::sort_splats_by_tile(
      screen_positions.data(), depths.data(), radii.data(), tile_touch_counts.data(),
      kSplatCount, kImageWidth, kImageHeight, kTileSize);

  ASSERT_EQ(result.sorted_keys.size(), expected_keys.size());
  for (size_t i = 0; i < result.sorted_keys.size(); ++i) {
    EXPECT_EQ(result.sorted_keys[i].sort_key, expected_keys[i].sort_key) << "entry " << i;
    EXPECT_EQ(result.sorted_keys[i].splat_index, expected_keys[i].splat_index) << "entry " << i;
  }

  // Every tile's range is internally sorted by depth (ascending — front to back) and covers
  // exactly the entries whose tile_id matches.
  for (uint32_t tile_id = 0; tile_id < result.tile_ranges.size(); ++tile_id) {
    const glint::splat::TileRange& range = result.tile_ranges[tile_id];
    for (uint32_t i = range.start; i < range.end; ++i) {
      EXPECT_EQ(result.sorted_keys[i].sort_key >> 32, tile_id) << "tile " << tile_id;
      if (i > range.start) {
        EXPECT_LE(result.sorted_keys[i - 1].sort_key, result.sorted_keys[i].sort_key);
      }
    }
  }
}
