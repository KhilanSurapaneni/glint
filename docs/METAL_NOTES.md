# Metal notes

Every GPU behavior that cost real debugging time, recorded so it doesn't cost it twice. Apple's
compute documentation is thinner than CUDA's — confident-sounding wrong answers are common, so
these are written up from what was actually verified on-device, not from docs alone.

## The Metal Toolchain isn't always installed with Xcode

`xcrun -sdk macosx metal --version` can fail even with a full Xcode install — on recent Xcode
versions, the Metal shader compiler is a separate downloadable component:

```bash
xcodebuild -downloadComponent MetalToolchain
```

If shader compilation fails with a cryptic "unable to find utility metal" (or similar) before
you've written a single line of Metal code, this is almost certainly why.

## `newDefaultLibrary()` doesn't work for plain executables

`MTL::Device::newDefaultLibrary()` searches the calling process's app bundle for a
`default.metallib`. A plain CLI executable (this project isn't a `.app`) has no bundle, so the
call silently returns nothing to search — not an error, just an empty result that fails later
in a confusing way.

Fix: load the compiled library explicitly, by path, resolved relative to the running
executable rather than the current working directory (which changes depending on how the
binary gets launched):

```cpp
// src/gpu/device.cpp
const std::filesystem::path metallib_path = executable_dir() / "default.metallib";
library_ = NS::TransferPtr(device_->newLibrary(
    NS::String::string(metallib_path.c_str(), NS::UTF8StringEncoding), &error));
```

`executable_dir()` uses `_NSGetExecutablePath` (from `<mach-o/dyld.h>`) rather than `argv[0]` or
`getcwd()` — both of those can be wrong depending on how/where the binary was launched from.

## MSL struct layout does not match a naive C++ struct

The single easiest way to get silent garbage (not a crash) instead of a real bug report. MSL's
alignment rules diverge from C++'s in ways that are easy to not think about:

- `float3` in MSL is **16-byte aligned and 16 bytes in size** — not the 12 bytes a C++
  `float[3]` or a naive `struct { float x, y, z; }` would give you.
- A struct's alignment in MSL is its largest member's alignment, same general rule as C++, but
  the padding that produces is easy to get wrong by hand for anything with mixed field sizes.

The fix that's actually held up: never write a shared struct by hand on both sides. Use
`simd::` types (`<simd/simd.h>`) on the C++ side and their MSL equivalents (identical names,
different implementation) on the Metal side, guarded by `__METAL_VERSION__` — the one macro
that's defined only when the Metal compiler itself is processing the file:

```cpp
// src/shaders/shared_types.h
#ifdef __METAL_VERSION__
  float4x4 camera_to_world;
#else
  simd::float4x4 camera_to_world;
#endif
```

Then a compile-time `static_assert(sizeof(T) == N && alignof(T) == M)` on the C++ side, and a
real round-trip test (`tests/test_layout.cpp`) that writes a known bit pattern from C++, reads
it back through an actual dispatched kernel, and checks field-by-field — because a
`static_assert` on size/alignment alone doesn't prove the *fields* landed at the offsets you
think they did, only that the total size matches.

## Metal's clip-space depth is [0, 1], not [-1, 1]

OpenGL's clip space maps depth to `[-1, 1]`; Metal's maps it to `[0, 1]`. This matters for the
sign of the perspective projection matrix's third and fourth rows — copying a projection matrix
formula from an OpenGL-oriented reference without adjusting for this convention produces a
matrix where `w` comes out with the wrong sign for points in front of the camera, and Metal's
perspective divide then clips every single point as "behind" the viewer.

Symptom was exactly that: a blank screen, no error, no validation-layer warning — the pipeline
was working correctly on data that looked, to the GPU, like it was all behind the camera.
Fixed by negating the affected rows so `w` comes out positive for points at positive view-space
depth, matching Metal's convention rather than OpenGL's.
