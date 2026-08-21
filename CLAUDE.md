# CLAUDE.md

Guidance for Claude Code working in this repository. Read this fully before your first edit.

---

## 1. What this project is

**glint** — a real-time, language-queryable 3D reconstruction system written in C++20, running entirely on Apple Silicon via Metal compute.

You point an iPhone at a room. The system ingests the RGB-D + pose stream, builds a 3D Gaussian Splatting map on the GPU in real time, tracks the camera against that map, distills CLIP features into the splats, and lets the user type `"coffee mug"` to have the matching region of the 3D scene highlight.

The end state is a single 60-second screen recording that needs no narration.

> Rename `glint` to whatever you like, but do it in one commit across `CMakeLists.txt`, the namespace, and the docs. Don't leave it half-renamed.

### Why it's built this way

This is a portfolio project targeting product-facing SWE roles at physical-AI companies. That shapes engineering decisions in ways that would be wrong for a research repo:

- **The demo is the deliverable.** A beautiful abstraction that doesn't render is worth zero. Bias toward a working vertical slice at every milestone.
- **Numbers are the second deliverable.** Every milestone must produce a line in `BENCHMARKS.md`. ms/frame, ATE in cm, peak GPU memory, splat count. "It works" is not a result.
- **Honesty is a feature.** `FAILURES.md` documenting where the system breaks is a hiring signal. Never delete a failure case to make the repo look cleaner.
- **No novelty claims.** This is a solid implementation of published work (3DGS, MonoGS/SplaTAM, LangSplat) on a platform that lacked one. Docs, commits, and comments must not imply research novelty.

### Non-goals

- Cross-platform support. macOS + Apple Silicon only. Do not add CUDA/Vulkan/DirectX paths.
- Beating published benchmarks on PSNR or ATE. Matching within a reasonable factor is success.
- A general-purpose engine, plugin system, or scene-graph abstraction. Resist all of it.
- Training anything large. Everything runs on one Mac, offline, for free.

---

## 2. Hard constraints

| Constraint | Consequence |
|---|---|
| Apple Silicon only (M1+) | Metal compute, not CUDA. Unified memory — no host/device copy step. |
| Everything free | No paid APIs, no cloud GPUs, no paid datasets. Free Apple dev account for sideloading. |
| Solo developer, ~12 weeks | Ship at every milestone. No speculative infrastructure. |
| Local-only | Model weights vendored or downloaded once to `assets/`, gitignored, with a fetch script. |

---

## 3. Stack

**Core:** C++20 · CMake ≥ 3.24 · Metal (MSL 3.0) via [metal-cpp](https://developer.apple.com/metal/cpp/) (header-only)

**Libraries:** Eigen 3.4 (CPU linear algebra) · Ceres (bundle adjustment, M3+) · Dear ImGui + GLFW (viewer) · GoogleTest · ONNX Runtime with CoreML EP (CLIP/SAM inference) · `stb_image` · `nlohmann/json`

**Capture:** ~200-line Swift/ARKit iOS app in `capture/ios/`. Streams `ARFrame` RGB, scene-depth (LiDAR), intrinsics, and `ARCamera.transform` over TCP or writes to a bundle for AirDrop.

**Dependency policy:** vendored via CMake `FetchContent`, pinned to explicit tags. Do not add a dependency without asking. If you think one is needed, propose it with the specific problem it solves and the alternative of writing it yourself. `brew` is acceptable only for OpenCV.

---

## 4. Repository layout

```
glint/
├── CLAUDE.md                  # this file
├── README.md                  # demo GIF first, then build instructions
├── BENCHMARKS.md              # append-only performance log
├── FAILURES.md                # append-only failure catalogue
├── CMakeLists.txt
├── cmake/
│   ├── CompileMetalShaders.cmake
│   └── Dependencies.cmake
├── src/
│   ├── core/                  # no Metal, no OpenCV. Pure math + types.
│   │   ├── types.hpp          # Camera, Frame, Pose, SplatSoA
│   │   ├── se3.hpp            # SE(3)/SO(3) exp, log, adjoint, Jacobians
│   │   ├── camera.hpp         # pinhole projection + its Jacobian
│   │   └── covariance.hpp     # 3D covariance from scale+quat, EWA 2D projection
│   ├── gpu/                   # metal-cpp RAII wrappers
│   │   ├── device.hpp/.cpp    # MTL::Device, queue, library loading
│   │   ├── buffer.hpp         # typed GPU buffer wrapper
│   │   ├── kernel.hpp         # pipeline state cache, dispatch helpers
│   │   └── profiler.hpp       # GPU timestamp scopes
│   ├── shaders/               # .metal sources — compiled to default.metallib
│   │   ├── shared_types.h     # ⚠️ included by BOTH C++ and MSL. See §8.
│   │   ├── preprocess.metal   # cull, project, 2D covariance, tile counts
│   │   ├── sort.metal         # radix sort over (tile_id, depth) keys
│   │   ├── raster_fwd.metal   # tile-based forward alpha compositing
│   │   ├── raster_bwd.metal   # backward pass
│   │   └── densify.metal      # clone/split/prune
│   ├── splat/
│   │   ├── model.hpp/.cpp     # SplatSoA lifecycle, densification policy
│   │   ├── rasterizer.hpp/.cpp# forward + backward orchestration
│   │   ├── optimizer.hpp/.cpp # Adam over splat parameters
│   │   └── loss.hpp/.cpp      # L1 + D-SSIM
│   ├── slam/
│   │   ├── tracker.hpp/.cpp   # frame-to-map pose optimization
│   │   ├── keyframes.hpp/.cpp # keyframe selection + sliding window
│   │   └── mapper.hpp/.cpp    # map update thread
│   ├── lang/
│   │   ├── features.hpp/.cpp  # CLIP feature extraction (offline)
│   │   ├── autoencoder.hpp    # 512-d → low-d compression
│   │   └── query.hpp/.cpp     # text embedding → per-splat relevancy
│   ├── io/
│   │   ├── ios_stream.cpp     # TCP receiver for capture app
│   │   ├── dataset.cpp        # Replica / TUM RGB-D / ScanNet loaders
│   │   └── ply.cpp            # 3DGS-compatible .ply export
│   ├── viewer/                # ImGui + GLFW, Metal-backed
│   └── main.cpp
├── tests/
│   ├── test_se3.cpp
│   ├── test_covariance.cpp
│   ├── test_gradients.cpp     # ⚠️ the most important test file. See §9.
│   ├── test_sort.cpp
│   └── golden/                # reference renders
├── tools/
│   ├── fetch_models.py        # ONNX export / download
│   ├── precompute_features.py # SAM + CLIP over keyframes → .npy cache
│   ├── eval_ate.py
│   └── plot_bench.py
├── capture/ios/               # Swift ARKit app
├── assets/                    # gitignored — models, datasets
└── docs/
    ├── ARCHITECTURE.md
    ├── METAL_NOTES.md         # every GPU gotcha you hit
    └── DERIVATIONS.md         # gradient math, written out by hand
```

**Layering rule, enforced strictly:** `core` → `gpu` → `splat` → `slam` → `lang` → `viewer`. Dependencies point one direction only. `core/` must compile and test without a GPU present.

---

## 5. Build and run

```bash
# configure + build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -GNinja
cmake --build build -j

# tests (always run before declaring work done)
ctest --test-dir build --output-on-failure

# run on a dataset
./build/glint --dataset assets/replica/room0 --headless --bench

# run live from phone
./build/glint --live --port 5555
```

**Debug builds are ~20× slower** for the optimizer. Use `RelWithDebInfo` by default; use `Debug` only when chasing a specific crash.

### Metal shader compilation

Shaders compile at build time into `default.metallib` via `cmake/CompileMetalShaders.cmake`:

```
xcrun -sdk macosx metal -c src/shaders/foo.metal -o build/foo.air -I src/shaders
xcrun -sdk macosx metallib build/*.air -o build/default.metallib
```

Two gotchas that will cost you an afternoon each:

1. **The Metal toolchain may not ship with Command Line Tools.** On recent Xcode versions it's a separate component: `xcodebuild -downloadComponent MetalToolchain`. If `xcrun -sdk macosx metal --version` fails, that's why.
2. **`newDefaultLibrary()` fails for non-bundled binaries.** We're a plain CLI executable, not a `.app`, so there's no bundle to search. Always load explicitly with `device->newLibrary(NS::String::string(path, NS::UTF8StringEncoding), &error)` and resolve the path relative to the executable.

### Debugging GPU work

```bash
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 ./build/glint ...
```

For anything visual, use Xcode's GPU frame capture. `printf` from MSL works but serializes execution and will mislead you about timing.

---

## 6. The pipeline, in detail

### 6.1 Splat representation

Stored as **structure-of-arrays** in a single GPU buffer region, never array-of-structs. Per splat:

| Field | Size | Storage | Notes |
|---|---|---|---|
| position | 3 | float3 | world space |
| rotation | 4 | float4 | quaternion, normalized on read |
| scale | 3 | float3 | **log-space**; `exp()` on read |
| opacity | 1 | float | **logit-space**; `sigmoid()` on read |
| SH coefficients | 48 | float | degree 3 → 16 coeffs × 3 channels |
| language feature | 3–16 | float | added at M4 |

Activation functions live in the parameterization, not the loss. Optimizing raw scale or raw opacity will produce negative scales and NaNs within a hundred iterations.

### 6.2 Forward rasterization

1. **Preprocess** (one thread per splat): frustum cull; project mean to screen; build 3D covariance `Σ = R S Sᵀ Rᵀ`; project to 2D via the EWA Jacobian `Σ' = J W Σ Wᵀ Jᵀ`; **add 0.3 to the 2D covariance diagonal** as an anti-aliasing low-pass filter (omitting this causes shimmering on thin structures); compute the screen-space radius (3σ) and the count of 16×16 tiles touched.
2. **Prefix sum** over tile counts to allocate the duplicated key array.
3. **Duplicate** each splat once per touched tile, key = `(uint64)tile_id << 32 | depth_bits`.
4. **Sort** keys ascending. One global radix sort gives per-tile front-to-back ordering for free.
5. **Identify tile ranges** by scanning for boundaries in the sorted keys.
6. **Render** (one threadgroup per tile, 16×16 = 256 threads): cooperatively load batches of splats into threadgroup memory, then each thread alpha-composites front-to-back over its pixel, terminating when transmittance `T < 1e-4`.

**Tile size is 16×16 and should stay there.** Apple GPUs have a SIMD width of 32 and 32 KB of threadgroup memory; 256 threads per group is a good occupancy point. Changing it means re-tuning the batch size in the raster kernel.

### 6.3 Backward pass

Iterate each tile **back-to-front**, reconstructing intermediate transmittance from the saved final `T` and each splat's alpha. Accumulate gradients to splat parameters with atomics.

Consequences you must design around:

- **Atomic accumulation makes the backward pass nondeterministic.** Float addition isn't associative, so bitwise-identical results across runs are impossible. All gradient tests use tolerances (§9), never exact equality.
- **Float atomics:** `atomic_float` `fetch_add` is available in MSL 3.0 on Apple silicon. Verify it on the target device at startup. If unavailable, fall back to fixed-point encoding into `atomic_uint` — and document the chosen scale factor in `METAL_NOTES.md`, because getting it wrong silently destroys gradients.

### 6.4 Densification

Every 100 iterations, between iteration 500 and 15000:

- **Clone** splats with view-space positional gradient magnitude > `2e-4` and scale below a threshold (under-reconstruction).
- **Split** splats above that gradient threshold whose scale is large (over-reconstruction) into two, dividing scale by ~1.6.
- **Prune** splats with `sigmoid(opacity) < 0.005`, or with screen radius above a max, or world scale above 10% of scene extent.
- **Reset opacity** to a low value every 3000 iterations to let floaters die.

These constants come from the original 3DGS paper and are a reasonable starting point, not gospel. If you change one, record the before/after in `BENCHMARKS.md`. Changing several at once and reporting one number is how you end up unable to explain your own results in an interview.

### 6.5 Tracking (M3)

Frame-to-map: freeze the map, optimize only the camera pose by minimizing photometric error between the render and the incoming frame.

- Parameterize the update on the **Lie algebra** `se(3)`, left perturbation: `T ← exp(δξ^) · T`. Never optimize raw matrix entries or Euler angles.
- Analytic Jacobian `∂(projected mean)/∂δξ` — write the derivation out longhand in `docs/DERIVATIONS.md` and verify it numerically before trusting it.
- Coarse-to-fine over an image pyramid; initialize from a constant-velocity motion model.
- New keyframe when translation, rotation, or covisibility crosses a threshold. Then run map optimization over a sliding window of recent keyframes.

Threading: tracking and mapping run on separate threads sharing the map behind a reader-writer lock, with the map double-buffered so tracking never blocks on a densification pass.

### 6.6 Language features (M4)

Storing 512-d CLIP features per splat is impossible — a million splats would need 2 GB. So:

1. **Offline** (`tools/precompute_features.py`): run mobile-SAM on each keyframe at three mask scales, crop each mask, encode with CLIP ViT-B/16, cache dense per-pixel features to `.npy`.
2. **Compress**: train a small scene-specific autoencoder mapping 512-d → 3-d (LangSplat's approach). Per-scene, takes minutes.
3. **Distill**: add the low-dim feature as an optimizable per-splat parameter, rasterized exactly like color, supervised against the cached feature maps.
4. **Query**: encode the text prompt with CLIP's text encoder, decode each splat's low-dim feature back to 512-d, compute a relevancy score, threshold, and highlight.

Do this offline first, entirely on CPU/CoreML, and only optimize it if there's time left. It is the last milestone for a reason.

---

## 7. C++ conventions

- **C++20.** Designated initializers, `std::span`, concepts where they clarify. No modules — CMake support isn't worth the pain here.
- **Naming:** `PascalCase` types, `snake_case` functions and variables, `member_` trailing underscore, `kConstant`, `NAMESPACE_MACROS`. Namespace `glint::`, sub-namespaces matching directories.
- **Formatting:** `.clang-format` at repo root, LLVM base, 100-column limit, 2-space indent. Run `clang-format -i` before every commit.
- **Errors:** exceptions for setup and configuration failures (shader compile, file not found, device unsupported). `std::expected` or an error enum in the per-frame path. Never throw from inside a frame loop.
- **Memory:** RAII everywhere. `NS::SharedPtr<MTL::Buffer>` for Metal objects. No raw `new`. No `shared_ptr` unless ownership is genuinely shared — it usually isn't.
- **Hot paths:** no allocation inside the per-frame loop. Pre-size all GPU buffers with a growth policy; log every reallocation at warn level so growth thrash is visible.
- **Comments:** this is a learning-focused solo project, so comment more than typical production style calls for. Add a short, plain-language comment on each meaningful chunk or non-obvious line — what it does, not just why — so the code is readable without outside context. Keep each comment lean: a line or two, not a paragraph; skip comments on lines that are already self-explanatory (e.g. `int width = 0;`). Math still gets a comment naming the paper and equation number.

---

## 8. ⚠️ The C++/MSL struct alignment trap

**This will bite you and the symptom is silent garbage, not a crash.**

`src/shaders/shared_types.h` is included by both C++ and Metal shaders and defines every struct crossing the boundary. MSL alignment rules differ from C++:

- `float3` in MSL is **16-byte aligned and 16 bytes in size**, not 12.
- `packed_float3` is 12 bytes, unaligned.
- MSL structs align to their largest member; padding rules will not match a naive C++ struct.

Rules:

1. Every shared struct uses explicit scalar members or `simd::` types from `<simd/simd.h>` on the C++ side.
2. Every shared struct gets a `static_assert(sizeof(T) == N && alignof(T) == M)` on the C++ side.
3. A test in `tests/test_layout.cpp` writes a known bit pattern from C++, reads it back through a trivial kernel, and asserts field-by-field equality. Run it whenever you touch `shared_types.h`.
4. Prefer flat scalar arrays over nested structs across the boundary. Boring beats clever.

---

## 9. Testing

`ctest` must be green before any work is called done. No exceptions.

### Tiers

1. **Math unit tests** (`core/`, CPU-only, fast). SE(3) round-trips: `log(exp(ξ)) ≈ ξ`. Quaternion→rotation→covariance consistency. Projection Jacobians against finite differences.
2. **Gradient tests** (`test_gradients.cpp`) — **the single most important file in this repo.** For every splat parameter, on a small synthetic scene (< 100 splats, 64×64 render):
   - Compute the analytic gradient from the backward kernel.
   - Compute the central-difference numerical gradient: `(L(θ+ε) − L(θ−ε)) / 2ε`, with ε tuned per parameter scale.
   - Assert relative error < 1e-3, with an absolute floor to avoid dividing by near-zero gradients.

   **Add the gradient test for a parameter in the same commit that adds the parameter.** A wrong gradient does not crash — it quietly makes reconstruction converge to something slightly worse, and you will not find it three weeks later by staring at renders.
3. **Golden-image tests.** Render a fixed scene from a fixed pose, compare against `tests/golden/*.png` with a PSNR threshold (not exact match — see the atomics note in §6.3). Regenerate goldens only deliberately, with the reason in the commit message.
4. **End-to-end smoke test.** 50 frames of a Replica sequence, assert final PSNR above a floor and no NaNs in any splat parameter. Runs in CI-equivalent time (< 2 min).

### NaN policy

Add a debug-only kernel that scans splat parameters for NaN/Inf after each optimizer step and hard-fails with the iteration number and offending index. Chasing a NaN to its origin iteration is a day's work; catching it at birth is a second.

---

## 10. Benchmarking

**Every milestone appends to `BENCHMARKS.md`.** A change with no measurement is not finished work.

Required fields per entry: date, commit hash, hardware (`M2 Pro, 16 GB`), dataset/scene, then:

| Metric | Unit | Notes |
|---|---|---|
| Total frame time | ms | median and p95, not mean |
| Preprocess / sort / raster fwd / raster bwd / optimizer | ms | GPU timestamps, must sum to ~total |
| Splat count | — | at end of sequence |
| Peak GPU memory | MB | |
| PSNR / SSIM | dB / — | held-out views |
| ATE RMSE | cm | M3+; Umeyama-aligned (`Eigen::umeyama`) |
| Tracking success rate | % | frames not requiring relocalization |

Use GPU timestamp sampling (`MTLCounterSampleBuffer`), not CPU wall-clock around a dispatch. Metal command submission is asynchronous; CPU-side timing measures encode time, not execution time, and will tell you a kernel is free when it isn't.

When you optimize something, the entry must state the before number, the after number, and **the specific reason for the change**. "Restructured the raster inner loop to read splat data from threadgroup memory in 256-splat batches, cutting L2 traffic — 24.1 ms → 9.3 ms" is the sentence you want to be able to say out loud. "Made it faster" is not.

---

## 11. FAILURES.md

Append-only. Every observed failure mode, with a screenshot or short clip:

- Motion blur during fast rotation → feature-poor frames → tracking divergence
- Specular and reflective surfaces → view-dependent floaters
- Textureless white walls → geometric ambiguity, splats drift in depth
- Thin structures (chair legs, cables) → under-reconstruction even after densification
- Depth sensor dropout on dark or transparent surfaces
- LiDAR range limits on large rooms

For each: symptom, hypothesized cause, whether you mitigated it, and what you'd do with more time.

**Never delete an entry to make the project look better.** This file is the reason a robotics engineer will believe the rest of the repo.

---

## 12. Milestones

Each milestone ends with: green tests, a `BENCHMARKS.md` entry, a screen recording in `docs/media/`, and a tagged commit.

### M0 — Skeleton (week 1)
CMake builds. metal-cpp links. A trivial compute kernel runs and returns correct results. GLFW window with ImGui. Dataset loader reads Replica.
**Exit:** a triangle renders and a kernel adds two buffers correctly.

### M1 — Point cloud viewer (weeks 2–3)
iOS capture app streams RGB-D + pose. C++ ingests it. Live point cloud renders in the viewer with orbit controls.
**Exit:** walk around a room, see the cloud build live. First demo video. This alone is already showable.

### M2 — 3D Gaussian Splatting core (weeks 4–7) ← **the hard part**
Forward rasterizer, backward pass, Adam, densification. Optimize a static set of posed frames into a splat model.
**Exit:** PSNR > 30 dB on a Replica scene; > 20 fps rendering at 640×480; every gradient test green; `.ply` exports and loads in a third-party 3DGS viewer.

> If you have six weeks total instead of twelve, **stop here.** "Real-time 3D Gaussian Splatting on Apple Silicon, in C++, with hand-written Metal kernels" is a complete, competitive project on its own.

### M3 — SLAM (weeks 8–10)
Pose optimization against the map. Keyframes, sliding-window mapping, separate tracking and mapping threads.
**Exit:** ATE RMSE reported on TUM RGB-D and on your own ARKit-ground-truthed capture. Tracking survives a full loop of a room.

### M4 — Language queries (weeks 11–12)
CLIP feature distillation, text query UI, relevancy highlighting.
**Exit:** type `"coffee mug"`, the mug lights up. Ten queries recorded, including the ones that fail.

---

## 13. Working agreements for Claude

**Do:**
- Read the relevant existing code before proposing changes. This codebase has specific conventions; match them.
- Work in vertical slices. A rough end-to-end path beats a perfect isolated module.
- Write the gradient test in the same commit as the gradient.
- Run `ctest` before saying something is done, and report actual output rather than assuming.
- Say when you're uncertain about Metal semantics. Apple's compute documentation is thinner than CUDA's and confident wrong answers are expensive here.
- Add a `METAL_NOTES.md` entry for every GPU behavior that surprised you.
- Keep functions short enough to read on one screen.

**Don't:**
- Add dependencies without asking.
- Add abstraction layers, plugin systems, or config frameworks. This project has exactly one target and one user.
- Rewrite working Metal kernels for style. They're hard to verify; churn there is expensive. Optimize only against a measured bottleneck.
- Touch `shared_types.h` without updating the `static_assert`s and running the layout test.
- Claim a performance improvement without a measurement.
- Write research-y language in comments or docs suggesting novelty. Cite the paper instead.
- Silently change densification or optimizer constants. They're tuned; changes need a benchmark entry.
- Delete or soften `FAILURES.md` entries.

**When stuck on GPU correctness,** the order is: check `shared_types.h` layout → check for a missing `threadgroup_barrier` → check atomics and race conditions → reduce to a 1-splat, 1-pixel case and hand-compute the expected value. Do not add random barriers until it works; that hides the bug and costs performance.

---

## 14. Reference

**Papers** (cite these, don't reinvent them):
- Kerbl et al., *3D Gaussian Splatting for Real-Time Radiance Field Rendering*, SIGGRAPH 2023 — the rasterizer and densification
- Zwicker et al., *EWA Splatting*, 2002 — the 2D covariance projection
- Matsuki et al., *Gaussian Splatting SLAM* (MonoGS), CVPR 2024 — pose optimization on the manifold
- Keetha et al., *SplaTAM*, CVPR 2024 — RGB-D splat SLAM
- Qin et al., *LangSplat*, CVPR 2024 — the autoencoder-compressed language features

**Datasets** (free): Replica, TUM RGB-D, ScanNet, plus your own ARKit captures.

**Docs:** `docs/ARCHITECTURE.md` (module boundaries and data flow), `docs/METAL_NOTES.md` (GPU gotchas), `docs/DERIVATIONS.md` (gradient math longhand).

---

## 15. README requirements

The README is read by hiring managers for roughly thirty seconds. In order:

1. **Demo GIF above the fold.** Language query highlighting a real object in a real room. No preamble.
2. One sentence: what it is and what it runs on.
3. Headline numbers: fps, resolution, splat count, ATE, hardware.
4. Architecture diagram.
5. What's implemented from scratch vs. what's a library — be explicit and honest. "Rasterizer, backward pass, and radix sort written in Metal; Eigen for CPU linear algebra; Ceres for bundle adjustment."
6. Build instructions that actually work on a clean machine. Test this on a friend's Mac.
7. Links to `BENCHMARKS.md` and `FAILURES.md`.
8. Limitations, stated plainly.

No "revolutionary." No "state-of-the-art." The numbers and the GIF make the argument.
