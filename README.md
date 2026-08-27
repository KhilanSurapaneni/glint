# glint

A real-time, language-queryable 3D reconstruction system, written in C++20, running entirely
on Apple Silicon via Metal compute. Point an iPhone at a room; the system builds a 3D Gaussian
Splatting map on the GPU in real time, tracks the camera against it, and lets you type a query
like `"coffee mug"` to highlight the matching region of the scene.

This README will get a demo GIF and headline numbers (fps, splat count, ATE) once the Gaussian
Splatting reconstruction (M2) exists to show — see **Status** below for where things actually
stand. Per-milestone recordings exist already: see **Milestone recordings**.

## Status: M1 complete (point cloud viewer)

- [x] **M0 — Skeleton.** CMake builds; metal-cpp links; a compute kernel runs and returns
      correct results; a GLFW+ImGui window renders a Metal triangle; the Replica dataset loader
      reads real RGB-D scenes into memory.
- [x] **M1 — Point cloud viewer.** Real Replica RGB-D, unprojected on the GPU into world-space
      points, rendered as an orbit-controllable point cloud. An optional iOS/ARKit capture app
      (`capture/ios/`) can feed the same viewer from a live phone or a saved capture file
      instead of Replica — see **Live/recorded capture** below.
- [ ] **M2 — 3D Gaussian Splatting core.** Forward/backward rasterizer, Adam optimizer,
      densification. The hard part.
- [ ] **M3 — SLAM.** Frame-to-map pose tracking, keyframes, sliding-window mapping.
- [ ] **M4 — Language queries.** CLIP feature distillation, text query UI.

Public datasets (Replica, TUM RGB-D, ScanNet) are the primary, required validation path for
every milestone in this project — phone capture is a parallel, optional track, never a
blocker. See `docs/CAPTURE_FORMAT.md` for the capture pipeline's wire format if you're
interested in that part specifically.

## What's implemented from scratch vs. a library

- **From scratch:** the GPU wrapper (`src/gpu/` — device/queue/buffer/pipeline management over
  metal-cpp), the pinhole unprojection kernel and orbit-camera viewer (`src/shaders/unproject.metal`,
  `src/viewer/`), the Metal↔AppKit and Metal↔ImGui bridges (the two places this project touches
  Objective-C++), the Replica dataset parser and the `.glcb` capture-bundle format/parser
  (`src/io/`), the iOS/ARKit capture app (`capture/ios/`, Swift/SwiftUI).
- **Libraries:** [metal-cpp](https://developer.apple.com/metal/cpp/) (Apple's C++ Metal
  bindings), [Eigen](https://eigen.tuxfamily.org/) (linear algebra), [GLFW](https://www.glfw.org/)
  (windowing), [Dear ImGui](https://github.com/ocornut/imgui) (UI), [stb_image](https://github.com/nothings/stb)
  (JPEG/PNG decode and encode), [GoogleTest](https://github.com/google/googletest) (testing).
  All vendored via CMake `FetchContent`, pinned to explicit versions — see
  `cmake/Dependencies.cmake`.

## Build

Requires macOS on Apple Silicon, full Xcode (not just Command Line Tools — the Metal shader
compiler needs it) with the Metal Toolchain component installed, and Homebrew's `cmake`/`ninja`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -GNinja
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/glint
```

Fetch a Replica scene first (`tools/fetch_replica.sh`, ~11.6 GB) — `./build/glint` needs
`assets/replica/Replica/room0` to exist, and the `Dataset`/`CaptureBundle` tests skip
gracefully if it isn't present.

`./build/glint` opens a window: 200 frames of Replica `room0`, unprojected into a real,
orbit-controllable point cloud. Left-drag to orbit, scroll to zoom.

### Live/recorded capture (optional, stretch)

```bash
./build/glint --live --port 5555     # listen for the iOS app over the same WiFi network
./build/glint --capture session.glcb # load a file the app saved instead of streaming live
```

Both point at the same `.glcb` wire format (`docs/CAPTURE_FORMAT.md`) the iOS app
(`capture/ios/GlintCapture`) writes. This path is code-complete on both ends but not yet
verified against real hardware — see **Limitations**.

## Milestone recordings

- [`docs/media/M0.mov`](docs/media/M0.mov) — the skeleton: a Metal triangle with a Dear ImGui
  overlay.
- [`docs/media/M1.mov`](docs/media/M1.mov) — the point cloud viewer: real Replica RGB-D,
  unprojected and orbit-controllable.

Not the flagship demo yet — that's the language-query highlight this README leads with, which
needs M4.

## Benchmarks and failures

- [`BENCHMARKS.md`](BENCHMARKS.md) — append-only performance log.
- [`FAILURES.md`](FAILURES.md) — append-only catalogue of observed failure modes.

## Limitations (current)

- No Gaussian Splatting, camera tracking, or language queries yet — this is a point cloud
  viewer, not a reconstruction system.
- The 200-frame load is a sequential prefix of Replica's trajectory, not spread across the
  room — dense coverage of one area, not the whole scene. See `src/io/dataset.cpp`.
- The orbit camera's default framing (`src/viewer/orbit_camera.hpp`) is hand-tuned to Replica
  `room0`'s specific coordinates, not a general auto-framing solution — it won't center
  correctly on a different scene without adjusting those defaults.
- Every depth pixel becomes a point, including invalid/zero-depth ones — there's no validity
  filtering yet, so a frame with depth dropout would show stray points at the camera's
  position rather than a gap.
- The iOS capture app and its C++ readers (`src/io/capture_bundle.cpp`, `src/io/ios_stream.cpp`)
  build and pass their own tests, but have never run against real ARKit hardware — live
  capture specifically depends on streaming uncompressed float32 depth over WiFi, which may
  not hold up at real frame rates (see `docs/CAPTURE_FORMAT.md`'s known limitation).
