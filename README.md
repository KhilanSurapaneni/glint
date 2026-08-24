# glint

A real-time, language-queryable 3D reconstruction system, written in C++20, running entirely
on Apple Silicon via Metal compute. Point an iPhone at a room; the system builds a 3D Gaussian
Splatting map on the GPU in real time, tracks the camera against it, and lets you type a query
like `"coffee mug"` to highlight the matching region of the scene.

This README will get a demo GIF and headline numbers (fps, splat count, ATE) once there's a
real reconstruction pipeline to show — see **Status** below for where things actually stand.

## Status: M0 complete (skeleton)

The build, GPU pipeline, and data-loading path all work end to end; no reconstruction, tracking,
or language features exist yet.

- [x] **M0 — Skeleton.** CMake builds; metal-cpp links; a compute kernel runs and returns
      correct results; a GLFW+ImGui window renders a Metal triangle; the Replica dataset loader
      reads real RGB-D scenes into memory. 5/5 tests passing.
- [ ] **M1 — Point cloud viewer.** iOS capture app streams RGB-D + pose; live point cloud
      renders in the viewer.
- [ ] **M2 — 3D Gaussian Splatting core.** Forward/backward rasterizer, Adam optimizer,
      densification. The hard part.
- [ ] **M3 — SLAM.** Frame-to-map pose tracking, keyframes, sliding-window mapping.
- [ ] **M4 — Language queries.** CLIP feature distillation, text query UI.

## What's implemented from scratch vs. a library

- **From scratch:** the GPU wrapper (`src/gpu/` — device/queue/buffer/pipeline management over
  metal-cpp), the Metal↔AppKit and Metal↔ImGui bridges (`src/viewer/`, the two places this
  project touches Objective-C++), the Replica dataset parser (`src/io/`).
- **Libraries:** [metal-cpp](https://developer.apple.com/metal/cpp/) (Apple's C++ Metal
  bindings), [Eigen](https://eigen.tuxfamily.org/) (linear algebra), [GLFW](https://www.glfw.org/)
  (windowing), [Dear ImGui](https://github.com/ocornut/imgui) (UI), [stb_image](https://github.com/nothings/stb)
  (image decoding), [GoogleTest](https://github.com/google/googletest) (testing). All vendored
  via CMake `FetchContent`, pinned to explicit versions — see `cmake/Dependencies.cmake`.

## Build

Requires macOS on Apple Silicon, full Xcode (not just Command Line Tools — the Metal shader
compiler needs it) with the Metal Toolchain component installed, and Homebrew's `cmake`/`ninja`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -GNinja
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/glint
```

`./build/glint` opens a window rendering a Metal triangle with a Dear ImGui overlay — proof the
graphics pipeline works, not yet the actual reconstruction viewer.

To exercise the dataset loader, fetch a Replica scene first (`tools/fetch_replica.sh`, ~11.6 GB) —
the `Dataset` test skips gracefully if it isn't present.

## Benchmarks and failures

- [`BENCHMARKS.md`](BENCHMARKS.md) — append-only performance log.
- [`FAILURES.md`](FAILURES.md) — append-only catalogue of observed failure modes. Empty right
  now for a real reason: nothing exists yet to fail in interesting ways.

## Limitations (current)

- No rendering of real reconstructed data — everything on screen today is a hardcoded triangle.
- No camera tracking, no SLAM, no language queries.
- No iOS capture app yet — data comes from the pre-recorded Replica dataset only.
