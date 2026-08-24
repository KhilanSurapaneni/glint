# M0 — Skeleton: Detailed Plan

Goal per `CLAUDE.md` §12: **CMake builds. metal-cpp links. A trivial compute kernel runs and
returns correct results. GLFW window with ImGui. Dataset loader reads Replica.**
**Exit:** a triangle renders and a kernel adds two buffers correctly.

This doc assumes zero prior knowledge of Metal, CMake, or this codebase. Every phase has:
what we're building, why, exact commands, and how to verify it worked. Work top to bottom —
each phase depends on the one before it. Check boxes as we go.

Legend:
- 🧑 **You do this** — GUI, App Store, sudo/password, or anything I structurally can't drive.
- 🤖 **I do this** — I'll run the commands via terminal once we reach that step.

---

## Phase 0 — Toolchain setup

Nothing in this project builds until this phase is done. Est. 30–90 min, mostly Xcode download time.

### 0.1 🧑 Install full Xcode ✅
You currently have Command Line Tools (`/Library/Developer/CommandLineTools`) but not the
full Xcode app. The Metal shader compiler toolchain is only obtainable through Xcode, not CLT
alone (`CLAUDE.md` §5 gotcha #1). Install Xcode from the Mac App Store — it's free, but large
(~10–15 GB) and requires being signed into an Apple ID.

- [x] Xcode installed from the App Store
- [x] Opened Xcode at least once (it may run a first-launch "installing additional components" step — let it finish)

### 0.2 🤖 Point the command-line tools at Xcode
```bash
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
xcode-select -p   # should print /Applications/Xcode.app/Contents/Developer
```
This requires your password (sudo) — run it yourself in a terminal if I can't prompt interactively.

- [x] `xcode-select -p` points into `Xcode.app`

### 0.3 🧑 Accept the Xcode license
```bash
sudo xcodebuild -license accept
```
- [x] License accepted (command exits with no error)

### 0.4 🤖 Download the Metal toolchain component
Recent Xcode ships the Metal compiler as a separate downloadable component.
```bash
xcodebuild -downloadComponent MetalToolchain
```
- [x] Command completes successfully

### 0.5 🤖 Verify the Metal compiler works
```bash
xcrun -sdk macosx metal --version
```
- [x] Prints a version instead of "unable to find utility"

### 0.6 🤖 Install build tools via Homebrew
Per `CLAUDE.md` §3, brew is only sanctioned for OpenCV as a *project dependency* — but build
tooling (cmake, ninja) isn't a project dependency, it's how we invoke the compiler, so brew is
fine here.
```bash
brew install cmake ninja
cmake --version   # need >= 3.24
ninja --version
```
- [x] `cmake --version` ≥ 3.24
- [x] `ninja --version` prints a version

**Phase 0 exit check:** `xcrun -sdk macosx metal --version`, `cmake --version`, `ninja --version`
all succeed.

---

## Phase 1 — Repository scaffolding

Create the directory structure from `CLAUDE.md` §4 (the M0-relevant subset only — we add
`slam/`, `lang/` etc. in later milestones, not now, per "no speculative infrastructure").

- [x] Create directories:
  ```
  cmake/
  src/core/
  src/gpu/
  src/shaders/
  src/io/
  src/viewer/
  tests/
  tests/golden/
  assets/            (gitignored)
  docs/media/
  ```
- [x] `.gitignore` — at minimum: `build/`, `assets/`, `.cache/`
- [x] `.clang-format` at repo root — LLVM base style, 100-column limit, 2-space indent (`CLAUDE.md` §7)
- [x] Decide: keep the name `glint`, or rename now? (§0 of `CLAUDE.md`: rename in one commit
      across `CMakeLists.txt` + namespace + docs if we do it — cheapest time to do it is before
      anything exists.)

---

## Phase 2 — Vendor metal-cpp

**What it is:** Apple's official header-only C++ bindings over the Objective-C Metal API —
lets us call Metal from `.cpp` without touching Objective-C++. Header-only, so "vendoring" it
just means fetching the headers at configure time.

- [x] `cmake/Dependencies.cmake`: add a `FetchContent_Declare` for metal-cpp, pinned to a
      specific tagged release (need to check Apple's metal-cpp page for the release matching
      the macOS SDK on this machine — we'll pick the tag when we get here rather than guess now)
- [x] Smoke-test: a `.cpp` file that just does `#include <Metal/Metal.hpp>` and compiles

---

## Phase 3 — Root CMake skeleton

- [x] Root `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.24)`, `project(glint CXX)`,
      `set(CMAKE_CXX_STANDARD 20)`, `CMAKE_CXX_STANDARD_REQUIRED ON` — done directly in Phase 2
- [x] `include(cmake/Dependencies.cmake)` — done in Phase 2
- [ ] `add_subdirectory(src)`, `add_subdirectory(tests)` — **deferred to Phase 4**, once there's
      more than one file to justify splitting into subdirectory CMakeLists.txt files (avoids
      empty speculative scaffolding per `CLAUDE.md` working agreements)
- [x] Configure + build confirmed working (Phase 2's smoke test build was this check, in practice)

---

## Phase 4 — Hello-GPU: a kernel that adds two buffers

This is the first real Metal code and proves the whole GPU path: shader compilation →
pipeline creation → dispatch → readback.

- [x] `src/shaders/shared_types.h` — minimal for now (can be nearly empty; real structs come
      in M2). Per `CLAUDE.md` §8, this file is included by both C++ and MSL — keep it scalar,
      no nested structs yet.
- [x] `src/shaders/add.metal` — a `kernel void add_arrays(...)` that does `c[i] = a[i] + b[i]`
- [x] `cmake/CompileMetalShaders.cmake` — custom build step:
  ```
  xcrun -sdk macosx metal -c src/shaders/*.metal -o build/*.air -I src/shaders
  xcrun -sdk macosx metallib build/*.air -o build/default.metallib
  ```
  wired in as a CMake custom command/target so it reruns when `.metal` files change
- [x] `src/gpu/device.hpp/.cpp` — wraps `MTL::CreateSystemDefaultDevice()`, a command queue,
      and **explicit** library loading via
      `device->newLibrary(NS::String::string(path, NS::UTF8StringEncoding), &error)`
      — not `newDefaultLibrary()`, which fails for non-bundled executables (`CLAUDE.md` §5 gotcha #2).
      Path is resolved relative to the built executable.
- [x] `src/gpu/buffer.hpp` — typed wrapper over `MTL::Buffer` (RAII, `NS::SharedPtr`, per §7)
- [x] `src/gpu/kernel.hpp` — pipeline-state cache + a dispatch helper
- [x] A test (`tests/test_gpu_add.cpp`, GoogleTest): allocate two buffers of known values,
      dispatch `add_arrays`, read back, assert every element equals the CPU-computed sum
- [x] `ctest --test-dir build --output-on-failure` — this test passes

**This is the milestone's "kernel adds two buffers correctly" exit criterion.**

---

## Phase 5 — Window: GLFW + ImGui, Metal-backed, triangle renders

- [x] `cmake/Dependencies.cmake`: `FetchContent` for GLFW (pinned `3.4`) and Dear ImGui
      (pinned `v1.92.9`)
- [x] `src/viewer/` — window creation via GLFW, `CAMetalLayer` bridge (`metal_layer_bridge.mm`,
      Objective-C++ — AppKit isn't reachable from metal-cpp)
- [x] Wire up ImGui's GLFW + Metal backends (`imgui_impl_glfw` called directly from `main.cpp`;
      `imgui_impl_metal` needed its own bridge file, `imgui_metal_bridge.mm` — its header uses
      raw Objective-C types plain C++ can't even parse)
- [x] Render loop: clear the drawable, draw one hardcoded triangle (proves the raw Metal
      render pipeline works, not just ImGui) — confirmed rendering correctly
- [x] overlay the ImGui demo window on top of the triangle — confirmed visible and interactive
- [x] `src/main.cpp` ties device init → viewer → render loop together
- [x] 🧑 Manual verification: triangle confirmed on screen (red/green/blue corners, interpolated)
- [x] 🧑 Manual verification: ImGui panel also visible over the triangle

**Phase 5 complete — both exit criteria ("GLFW window with ImGui", "a triangle renders") met.**

*Along the way: fixed a real design gap in `cmake/CompileMetalShaders.cmake` — originally each
target would've tried to build its own `default.metallib`, which breaks the moment a second
shader-using target exists. Refactored into `glint_compile_metal_shaders()` (called once,
project-wide) + `glint_depends_on_metallib(TARGET)`, matching `CLAUDE.md`'s own "one shared
default.metallib" model.*

---

## Phase 6 — Dataset loader reads Replica

- [x] Get one Replica scene into `assets/replica/` (gitignored). Downloaded and extracted —
      8 scenes (`office0`-`4`, `room0`-`2`), verified against real files, not just docs:
      `results/frame*.jpg` (1200×680 RGB) + `results/depth*.png` (1200×680, 16-bit) pairs,
      2000 of each in `room0`, matching `traj.txt`'s 2000 lines (16 floats each, row-major 4×4
      camera-to-world, confirmed by the trailing `0 0 0 1` row). Intrinsics confirmed from
      NICE-SLAM's own config: `fx=fy=600.0, cx=599.5, cy=339.5, png_depth_scale=6553.5`.
      **Non-obvious catch found in the reference loader code**: parsed poses need their Y and Z
      columns negated (coordinate-convention fix) — would've silently produced a wrong/mirrored
      trajectory if skipped.
  - [x] `tools/fetch_replica.sh` — documents the exact fetch, with the verified structure/
        intrinsics/gotcha written into the script's own header comment
- [x] `src/core/types.hpp` — minimal `Camera`, `Frame`, `Pose` structs needed to hold a loaded
      frame. Vendored Eigen (`3.4.1`), fetching only its source (not its own heavy
      CMakeLists.txt / test suite) via a nonexistent `SOURCE_SUBDIR` — the CMake-endorsed
      replacement for the now-deprecated `FetchContent_Populate` two-step form.
      `tests/test_core_types.cpp` proves `core/` builds/runs with zero GPU dependency — no
      `glint_depends_on_metallib` call for this test target, on purpose.
- [x] `src/io/dataset.cpp` — Replica-format loader (RGB + depth + intrinsics + poses). Vendored
      `stb_image` (pinned to a commit hash — this repo has no release tags). Changed
      `core::Frame::depth` from raw `uint16_t` to `float` meters, converting with
      `png_depth_scale` once at load time rather than leaking that dataset detail downstream.
      Applies the verified Y/Z column-negation fix to every parsed pose.
- [x] Test: load the scene, assert frame count > 0 and first frame's intrinsics/pose are sane
      (non-zero, finite). `tests/test_dataset.cpp` — `GTEST_SKIP()`s gracefully if
      `assets/replica/` isn't present (points at `tools/fetch_replica.sh`), otherwise loads the
      real `room0` scene and checks: frame count == 2000, intrinsics match the known constants,
      first pose is finite and not identity, RGB/depth buffer sizes are exactly right.

**Phase 6 complete — dataset loader reads Replica, proven on real data.**

---

## Phase 7 — Tests wired end-to-end

- [ ] `FetchContent` for GoogleTest in `cmake/Dependencies.cmake`
- [ ] `tests/CMakeLists.txt` registers `test_gpu_add`, `test_dataset`, (and stub `test_se3.cpp`
      / `test_covariance.cpp` if any core math already exists — otherwise these arrive with M2)
- [ ] `ctest --test-dir build --output-on-failure` — everything green

---

## Phase 8 — Exit checklist

Per `CLAUDE.md` §12, every milestone ends with green tests, a `BENCHMARKS.md` entry, a
screen recording, and a tagged commit.

- [ ] `cmake -S . -B build -GNinja` configures cleanly from scratch
- [ ] `cmake --build build -j` builds cleanly from scratch
- [ ] `ctest --test-dir build --output-on-failure` — all green
- [ ] Triangle + ImGui window visually confirmed on screen
- [ ] Kernel add-buffers test numerically correct
- [ ] Replica scene loads with sane frame count/intrinsics
- [ ] `BENCHMARKS.md` seeded with a first entry (date, commit hash, `M4 Max, 36 GB`, and
      whatever's measurable at this stage — e.g. kernel dispatch+readback latency for the
      add-buffers test; real GPU-timestamp profiling per §10 lands with the rasterizer in M2)
- [ ] Short screen recording saved to `docs/media/`
- [ ] `git add` reviewed, commit created
- [ ] 🧑 confirm before tagging/pushing anything — tagging is fine locally, pushing is your call

---

## Troubleshooting quick-reference (from `CLAUDE.md` §5, §8)

| Symptom | Likely cause |
|---|---|
| `xcrun: error: unable to find utility "metal"` | Metal toolchain component not downloaded (0.4), or `xcode-select` still points at CLT (0.2) |
| Kernel runs but output is garbage, no crash | Check `shared_types.h` struct layout/alignment first (§8) — MSL `float3` is 16 bytes, not 12 |
| `newDefaultLibrary()` returns null | We're not an `.app` bundle — must load the `.metallib` by explicit path (§5 gotcha #2) |
| CMake can't find a `FetchContent` dependency | Usually a network/proxy issue, or a moved/retagged upstream release — check the pinned tag still exists |
| Debug build painfully slow | Expected (~20× per §5) — use `RelWithDebInfo` unless actively debugging a crash |

---

## Where we are right now

Nothing built yet. Suggested next action: start Phase 0 together, step 0.1 (Xcode install is
the one long-pole GUI step — good to kick off first and do other phases' *planning* while it
downloads).
