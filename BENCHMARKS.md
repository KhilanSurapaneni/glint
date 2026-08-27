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

---

## 2026-08-27 — M1 complete

**Commit:** pending (not yet committed at time of writing — update this line once it is)
**Hardware:** Apple M4 Max, 36 GB RAM, macOS 26.6.2

M1 adds a real unprojected point cloud, but still no rasterizer/tracker, so the standard
per-frame timing breakdown stays N/A until M2. What's real and measured (or honestly
extrapolated) today:

| Metric | Value | Notes |
|---|---|---|
| Points rendered | 163,200,000 | 200 frames × 1200×680 pixels/frame, `main.cpp`'s default load — every depth pixel becomes a point, no validity filtering yet (see `FAILURES.md`/README limitations) |
| GPU memory (position+color buffers, resident for the run) | ≈ 3.65 GiB | 200 frames × (816,000 px × 6 floats × 4 bytes) |
| CPU memory (raw RGB+depth, resident for the run) | ≈ 1.06 GiB | 200 frames × (816,000×3 B RGB + 816,000×4 B depth) |
| 200-frame load time | ≈ 3–10 s (extrapolated) | Not freshly timed — derived from M0's measured 15.0–49.4 ms/frame Replica decode rate (warm/cold OS cache) at 200 frames instead of 2000. Flagged as an extrapolation, not a new measurement. |
| Rendering frame time / fps | median ≈ 16.9 fps, p95 ≈ 21.7 fps, over 47 one-second windows (steady state) | App-level CPU wall-clock (`std::chrono`, printed each second in `main.cpp`), not GPU-timestamp profiling — appropriate here since this measures the render loop's overall pacing, not an individual kernel's cost; that distinction (and why per-kernel timing needs `MTLCounterSampleBuffer` instead) matters once the rasterizer's stages exist to profile in M2. First frame excluded as a one-off startup transient (8.4 fps, GPU pipeline/shader warm-up). Every one of the 163.2M points gets vertex-shaded every frame regardless of what's actually visible on screen — no view-frustum culling exists yet, so this number is a floor, not a ceiling, for a scene this dense. |
| Test suite | 9/9 passing | `ctest --test-dir build`, includes the new `CaptureBundle` round-trip tests |

**Rasterizer/tracker metrics (splat count, PSNR/SSIM, ATE RMSE, tracking success rate):** N/A —
still M2/M3 territory. Note this fps number isn't the same metric as M2's ">20 fps at 640×480"
exit bar — M1 has no such requirement, and the two aren't apples-to-apples (different
resolution, and 163.2M raw points vs. however many splats M2 ends up with).
