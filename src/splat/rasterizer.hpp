#pragma once

#include "core/types.hpp"
#include "gpu/device.hpp"
#include "splat/model.hpp"

#include <vector>

namespace glint::splat {

// A rendered image: RGB, row-major, `width*height*3` floats — raw accumulated color, not
// clamped or gamma-corrected. A pixel no splat ever reached is exactly (0, 0, 0).
struct RenderedImage {
  std::vector<float> rgb;
  int width = 0;
  int height = 0;
};

// The complete forward pass: preprocess (Phase 2) -> sort (Phase 3) -> raster (Phase 4), given
// a splat model and the camera to render it from. One call is the whole "render this view"
// operation the rest of the project (loss, training, viewing) builds on.
RenderedImage render(gpu::Device& device, const SplatSoA& splats, const core::Camera& camera,
                      const core::Pose& pose);

}  // namespace glint::splat
