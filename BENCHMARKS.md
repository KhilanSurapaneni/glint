# Benchmarks

Append-only performance log. Every entry states hardware, dataset/scene, and the actual
measured numbers. When something gets optimized, the entry states the before number, the
after number, and the specific reason — never just "made it faster."

---

## 2026-08-24 — M0 complete

**Commit:** `e158132`
**Hardware:** Apple M4 Max, 36 GB RAM, macOS 26.5.1

M0 has no rasterizer, splat model, or tracker yet, so most of the standard metric table below
is genuinely not applicable — it will start filling in from M2 onward. What's real and
measured today:

| Metric | Value | Notes |
|---|---|---|
| GPU compute kernel correctness | 1024/1024 elements correct | `add_arrays`, verified by `GpuAdd.AddsArraysCorrectly` |
| GPU kernel dispatch timing | not yet instrumented | proper `MTLCounterSampleBuffer` GPU-timestamp profiling arrives with the rasterizer in M2 — no crude CPU wall-clock substitute is reported here, to avoid a misleading number |
| Replica `room0` load time (cold, uncached) | 98.77 s for 2000 frames ≈ 49.4 ms/frame | `stb_image` JPEG + 16-bit PNG decode, single-threaded |
| Replica `room0` load time (warm OS file cache) | 30.08 s for 2000 frames ≈ 15.0 ms/frame | same data, run immediately after the cold run — the ~3x difference is disk/cache effects, not a code change |
| Test suite | 5/5 passing | `ctest --test-dir build`, confirmed after a full clean-room rebuild (`rm -rf build` first) |

**Total frame time · preprocess/sort/raster fwd/raster bwd/optimizer breakdown · splat count ·
peak GPU memory · PSNR/SSIM · ATE RMSE · tracking success rate:** N/A — none of these exist
until the rasterizer (M2) and tracker (M3) are built.
