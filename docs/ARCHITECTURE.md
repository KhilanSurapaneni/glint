# Architecture

Module boundaries and data flow, as of M1. Layering is strict and one-directional:

```
core → gpu → splat → slam → lang → viewer
```

`core/` never depends on anything below it in this list; `gpu/` depends only on `core`; and so
on. `io/` sits beside this chain — it depends only on `core` (it turns external data into
`core::Frame`/`core::Camera`) and is used by `main.cpp` alongside `gpu`/`viewer`, not by any of
the pipeline stages themselves.

## Modules

**`core/`** — pure math and types, no Metal, no OpenCV, compiles and tests without a GPU
present. Today: `Camera`, `Pose`, `Frame` (`src/core/types.hpp`). SE(3) math and the
projection Jacobian arrive with M2/M3.

**`gpu/`** — thin RAII wrappers over metal-cpp. `Device` (owns the `MTL::Device`, command
queue, and the compiled shader library — see `device.cpp`), `Buffer<T>` (a typed, fixed-size
GPU buffer), `Kernel` (compute pipeline state + dispatch helper). Nothing here knows what a
splat or a point cloud is; it only knows how to run a named kernel over a grid of threads.

**`io/`** — turns external data into `core::Frame`/`core::Camera`. Three independent sources,
one shared parsing layer for the two that share a wire format:

- `dataset.cpp` — reads Replica's `results/frame*.jpg` + `depth*.png` + `traj.txt` layout.
  Fully independent of the `.glcb` format below; Replica's on-disk layout isn't something we
  control, so there's nothing to share with it.
- `capture_format.{hpp,cpp}` — the `.glcb` wire format's actual read/write logic (see
  `docs/CAPTURE_FORMAT.md`), written once against an abstract byte source/sink so both
  transports below reuse it verbatim.
- `capture_bundle.cpp` — reads a complete `.glcb` file (`ifstream`-backed byte source).
- `ios_stream.cpp` — reads a live `.glcb` stream over TCP (`recv()`-backed byte source, on a
  background thread so the caller isn't blocked waiting on the phone).

**`shaders/`** — `.metal` sources, compiled at build time into one `default.metallib`
(`cmake/CompileMetalShaders.cmake`). `shared_types.h` is the one file both C++ and MSL include,
for structs that cross that boundary (see `docs/METAL_NOTES.md` for why that's load-bearing).
Today: `unproject.metal` (pinhole unprojection — pixel + depth → world-space point) and
`point_cloud.metal` (the point-primitive vertex/fragment pair the viewer renders with).

**`viewer/`** — GLFW window + Metal drawable surface (`metal_layer_bridge`), Dear ImGui's Metal
backend (`imgui_metal_bridge`), and `OrbitCamera` (mouse-driven view/projection matrix). The
two `.mm` (Objective-C++) files in the project live here — everywhere else is plain C++.

**`main.cpp`** — the only place all of the above meet. Picks a point source (Replica dataset by
default; `--live`/`--capture` for the optional capture stretch track), unprojects every frame
on the GPU into position/color buffers, and runs the render loop.

## Data flow (M1)

```
                    ┌─── io::load_replica_scene() ───┐
                    │        (Replica on disk)        │
                    ├─── io::load_capture_bundle() ───┤──→ core::Frame (rgb, depth, pose)
                    │         (.glcb file)             │         + core::Camera
                    └─── io::IosStreamServer ──────────┘
                              (.glcb over TCP)
                                     │
                                     ▼
                     gpu::Kernel "unproject_pixel"
                (one thread per pixel: pixel+depth → world point)
                                     │
                                     ▼
                  positions/colors buffers (one pair per frame)
                                     │
                                     ▼
                shaders/point_cloud.metal (vertex: view-project,
                    fragment: flat color) — one draw call per frame
                                     │
                                     ▼
                      viewer::OrbitCamera-controlled window
```

Every frame's points are kept in their own buffer pair rather than merged into one big buffer —
simple, and it's what makes `--live`'s incremental growth (new frames just `push_back` a new
pair) work without touching the frames already on screen.

## Where this goes next (M2)

M2 replaces "one point per depth pixel" with the actual splat representation (`splat/model.hpp`)
and the forward/backward rasterizer (`splat/rasterizer.hpp`, `shaders/preprocess.metal` /
`sort.metal` / `raster_fwd.metal` / `raster_bwd.metal`), sitting between `gpu` and `slam` in the
layering chain above.
