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

### 0.1 🧑 Install full Xcode
You currently have Command Line Tools (`/Library/Developer/CommandLineTools`) but not the
full Xcode app. The Metal shader compiler toolchain is only obtainable through Xcode, not CLT
alone (`CLAUDE.md` §5 gotcha #1). Install Xcode from the Mac App Store — it's free, but large
(~10–15 GB) and requires being signed into an Apple ID.

- [ ] Xcode installed from the App Store
- [ ] Opened Xcode at least once (it may run a first-launch "installing additional components" step — let it finish)

### 0.2 🤖 Point the command-line tools at Xcode
```bash
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
xcode-select -p   # should print /Applications/Xcode.app/Contents/Developer
```
This requires your password (sudo) — run it yourself in a terminal if I can't prompt interactively.

- [ ] `xcode-select -p` points into `Xcode.app`

### 0.3 🧑 Accept the Xcode license
```bash
sudo xcodebuild -license accept
```
- [ ] License accepted (command exits with no error)

### 0.4 🤖 Download the Metal toolchain component
Recent Xcode ships the Metal compiler as a separate downloadable component.
```bash
xcodebuild -downloadComponent MetalToolchain
```
- [ ] Command completes successfully

### 0.5 🤖 Verify the Metal compiler works
```bash
xcrun -sdk macosx metal --version
```
- [ ] Prints a version instead of "unable to find utility"

### 0.6 🤖 Install build tools via Homebrew
Per `CLAUDE.md` §3, brew is only sanctioned for OpenCV as a *project dependency* — but build
tooling (cmake, ninja) isn't a project dependency, it's how we invoke the compiler, so brew is
fine here.
```bash
brew install cmake ninja
cmake --version   # need >= 3.24
ninja --version
```
- [ ] `cmake --version` ≥ 3.24
- [ ] `ninja --version` prints a version

**Phase 0 exit check:** `xcrun -sdk macosx metal --version`, `cmake --version`, `ninja --version`
all succeed.

---

## Phase 1 — Repository scaffolding

Create the directory structure from `CLAUDE.md` §4 (the M0-relevant subset only — we add
`slam/`, `lang/` etc. in later milestones, not now, per "no speculative infrastructure").

- [ ] Create directories:
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
- [ ] `.gitignore` — at minimum: `build/`, `assets/`, `.cache/`
- [ ] `.clang-format` at repo root — LLVM base style, 100-column limit, 2-space indent (`CLAUDE.md` §7)
- [ ] Decide: keep the name `glint`, or rename now? (§0 of `CLAUDE.md`: rename in one commit
      across `CMakeLists.txt` + namespace + docs if we do it — cheapest time to do it is before
      anything exists.)

---

## Phase 2 — Vendor metal-cpp

**What it is:** Apple's official header-only C++ bindings over the Objective-C Metal API —
lets us call Metal from `.cpp` without touching Objective-C++. Header-only, so "vendoring" it
just means fetching the headers at configure time.

- [ ] `cmake/Dependencies.cmake`: add a `FetchContent_Declare` for metal-cpp, pinned to a
      specific tagged release (need to check Apple's metal-cpp page for the release matching
      the macOS SDK on this machine — we'll pick the tag when we get here rather than guess now)
- [ ] Smoke-test: a `.cpp` file that just does `#include <Metal/Metal.hpp>` and compiles

---

## Phase 3 — Root CMake skeleton

- [ ] Root `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.24)`, `project(glint CXX)`,
      `set(CMAKE_CXX_STANDARD 20)`, `CMAKE_CXX_STANDARD_REQUIRED ON`
- [ ] `include(cmake/Dependencies.cmake)`
- [ ] `add_subdirectory(src)`, `add_subdirectory(tests)`
- [ ] Configure + build with **nothing in it yet**, just to confirm the CMake plumbing itself works:
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -GNinja
  cmake --build build -j
  ```
- [ ] Empty build succeeds

---

## Phase 4 — Hello-GPU: a kernel that adds two buffers

This is the first real Metal code and proves the whole GPU path: shader compilation →
pipeline creation → dispatch → readback.

- [ ] `src/shaders/shared_types.h` — minimal for now (can be nearly empty; real structs come
      in M2). Per `CLAUDE.md` §8, this file is included by both C++ and MSL — keep it scalar,
      no nested structs yet.
- [ ] `src/shaders/add.metal` — a `kernel void add_arrays(...)` that does `c[i] = a[i] + b[i]`
- [ ] `cmake/CompileMetalShaders.cmake` — custom build step:
  ```
  xcrun -sdk macosx metal -c src/shaders/*.metal -o build/*.air -I src/shaders
  xcrun -sdk macosx metallib build/*.air -o build/default.metallib
  ```
  wired in as a CMake custom command/target so it reruns when `.metal` files change
- [ ] `src/gpu/device.hpp/.cpp` — wraps `MTL::CreateSystemDefaultDevice()`, a command queue,
      and **explicit** library loading via
      `device->newLibrary(NS::String::string(path, NS::UTF8StringEncoding), &error)`
      — not `newDefaultLibrary()`, which fails for non-bundled executables (`CLAUDE.md` §5 gotcha #2).
      Path is resolved relative to the built executable.
- [ ] `src/gpu/buffer.hpp` — typed wrapper over `MTL::Buffer` (RAII, `NS::SharedPtr`, per §7)
- [ ] `src/gpu/kernel.hpp` — pipeline-state cache + a dispatch helper
- [ ] A test (`tests/test_gpu_add.cpp`, GoogleTest): allocate two buffers of known values,
      dispatch `add_arrays`, read back, assert every element equals the CPU-computed sum
- [ ] `ctest --test-dir build --output-on-failure` — this test passes

**This is the milestone's "kernel adds two buffers correctly" exit criterion.**

---

## Phase 5 — Window: GLFW + ImGui, Metal-backed, triangle renders

- [ ] `cmake/Dependencies.cmake`: `FetchContent` for GLFW and Dear ImGui (pinned tags)
- [ ] `src/viewer/` — window creation via GLFW, attach a `CAMetalLayer` to the native window
      handle for Metal-backed presentation
- [ ] Wire up ImGui's GLFW + Metal backends (`imgui_impl_glfw`, `imgui_impl_metal`)
- [ ] Render loop: clear the drawable, draw one hardcoded triangle (proves the raw Metal
      render pipeline works, not just ImGui), overlay the ImGui demo window
- [ ] `src/main.cpp` ties device init → viewer → render loop together
- [ ] 🧑 Manual verification: run `./build/glint` and visually confirm a window opens showing
      a triangle and an ImGui panel

**This is the milestone's "GLFW window with ImGui" and "a triangle renders" exit criteria.**

---

## Phase 6 — Dataset loader reads Replica

- [ ] Get one Replica scene into `assets/replica/` (gitignored). We'll figure out the current
      correct download source together when we reach this step rather than guessing a URL now
      — Replica is distributed via a fetch script from the original Replica/NICE-SLAM/SplaTAM
      research repos, and the exact working link/method should be verified live, not assumed.
  - [ ] `tools/fetch_models.py`-style script or documented manual step, per `CLAUDE.md` §4
- [ ] `src/core/types.hpp` — minimal `Camera`, `Frame`, `Pose` structs needed to hold a loaded frame
- [ ] `src/io/dataset.cpp` — Replica-format loader (RGB + depth + intrinsics + poses)
- [ ] Test: load the scene, assert frame count > 0 and first frame's intrinsics/pose are sane
      (non-zero, finite)

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
