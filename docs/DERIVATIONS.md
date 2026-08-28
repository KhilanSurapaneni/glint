# Derivations

Gradient and Jacobian math, written out by hand before being trusted in a kernel — the
standard this project holds itself to for anything downstream code depends on being right.

---

## 3D covariance from scale + rotation (`core::covariance_3d`)

A splat's shape is a 3×3 covariance matrix Σ. We don't store Σ's 6 numbers directly —
optimizing them freely gives no guarantee the result is even a valid covariance (symmetric,
positive semi-definite). Instead we store a rotation and a per-axis scale, and build Σ from
them so an invalid shape is structurally impossible.

**Start with an axis-aligned blob.** Stretched by `(sx, sy, sz)` along the x/y/z axes, its
covariance is `S² = diag(sx², sy², sz²)` — variance is squared spread, same as in ordinary
statistics.

**Now rotate it.** Under a linear map `A`, a covariance transforms as `Σ' = A Σ Aᵀ` (if a
random vector `X` has covariance `Σ`, then `Y = AX` has covariance `A Σ Aᵀ` — this is the
standard change-of-variables rule for covariance). Rotating the axis-aligned blob by rotation
matrix `R`:

```
Σ = R (S²) Rᵀ = R S Sᵀ Rᵀ        (S is diagonal, so S = Sᵀ, hence S² = S Sᵀ)
```

Equivalently, with `M = R S`: `Σ = M Mᵀ` — this is what the code actually computes, one
matrix multiply cheaper than building `S²` and then two more multiplies.

**Why this guarantees a valid covariance for any input:** `M Mᵀ` is symmetric and positive
semi-definite for *any* matrix `M` — this is a general linear-algebra fact, not specific to
rotations. So no matter what rotation or nonnegative scale goes in, the result is always a
legitimate covariance. This is the same reasoning that motivates storing scale in log-space
(always positive) and rotation as a normalized quaternion (always a true rotation, see
`splat::normalize_quaternion`) — the parameterization itself rules out invalid states, rather
than relying on the optimizer to avoid them.

---

## The EWA projection Jacobian (`core::covariance_2d`)

We need a splat's 2D footprint once photographed. The real pinhole projection of a
camera-space point `p = (x, y, z)` is:

```
proj(p) = (fx·x/z, fy·y/z)
```

This is **nonlinear** — it divides by `z`. Covariance only transforms simply (`Σ' = A Σ Aᵀ`)
under a *linear* map, so `proj` can't be applied to Σ directly.

**The approximation (Zwicker et al. 2002, EWA splatting):** a splat is small relative to its
distance from the camera, so locally — right around the splat's own position — the nonlinear
projection looks approximately like a straight line, the same way a smooth curve looks
approximately straight if you zoom in close enough. We use the projection function's own
derivative (its Jacobian) *at that one point* as a stand-in linear map. This is only valid in
a small neighborhood around where it's evaluated — the tighter the splat relative to its
distance from the camera, the better the approximation holds. A splat that's genuinely huge
relative to its distance (rare in practice, since densification keeps splats small) would make
this approximation break down.

**The Jacobian itself** — partial derivatives of each output w.r.t. each input, via the
quotient rule on `x/z` and `y/z`:

```
∂proj_x/∂x = fx/z      ∂proj_x/∂y = 0        ∂proj_x/∂z = -fx·x/z²
∂proj_y/∂x = 0         ∂proj_y/∂y = fy/z     ∂proj_y/∂z = -fy·y/z²
```

Padded into a 3×3 matrix `J` with a zero third row — the projected depth's own differential
isn't part of a 2D footprint, so it's dropped rather than computed.

**Putting it together.** With `W` the 3×3 rotation part of the world-to-camera transform
(translation doesn't affect covariance — only rotation, and the point the Jacobian is
evaluated at, do) and `Σ` the splat's world-space covariance:

```
Σ' = J W Σ Wᵀ Jᵀ
```

`W Σ Wᵀ` first rotates the world-space covariance into camera space (same change-of-variables
rule as above); `J (...) Jᵀ` then applies the local-linear approximation of the projection.
The result is 3×3, but only its top-left 2×2 block corresponds to actual screen-space `(x, y)`
spread — that's the part `covariance_2d` returns.

**Verified, not just implemented from the paper on faith:**
`tests/test_covariance.cpp`'s `ProjectionJacobianMatchesFiniteDifferences` independently
re-derives this same Jacobian by central-differencing the real, nonlinear `proj()` function and
checks it agrees with the analytic formula above — no Jacobian in this project gets trusted
without a numerical check first.
