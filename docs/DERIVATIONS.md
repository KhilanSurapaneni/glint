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

---

# The backward pass

Everything below answers one question: given how wrong the rendered image came out
(`∂L/∂C`, one gradient per pixel per channel), how much is each splat parameter responsible,
and in which direction should it move? This is the reference `tests/test_gradients.cpp`
verifies against — the derivation comes first, the kernel implements it, and the
central-difference tests confirm the two agree. The tests are there to *verify* this math,
not to discover what it should have been.

## 0. Setup, notation, and scope

Forward model for one pixel, splats indexed `i = 1..n` in front-to-back (sorted) order:

```
Δ_i   = p − μ_i                    pixel center minus splat i's 2D screen mean
d_i   = Δ_iᵀ Σ'_i⁻¹ Δ_i            squared Mahalanobis distance (Σ'_i is the 2D covariance)
α_i   = o_i · exp(−½ d_i)          opacity times Gaussian falloff, clamped to ≤ 0.99
T_i   = ∏_{j<i} (1 − α_j)          transmittance surviving to splat i  (T_1 = 1)
C     = Σ_i T_i α_i c_i            the pixel's final color (background is 0 — no extra term)
```

with `o_i = sigmoid(ℓ_i)` (stored logit), `c_i` the SH-evaluated view-dependent color,
`μ_i` and `Σ'_i` from the EWA projection above, and the splat's camera-space position
`t = (x, y, z) = W p_world + translation` (`W` = view-matrix rotation part).

**What is deliberately treated as constant (non-differentiable), and why that's sound:**

- **Sort order.** Depth changes continuously with position, but *ordering* changes discretely.
  The compositing sum is piecewise-smooth in the parameters; within a piece (no order flip)
  the gradients below are exact. Central-difference tests use ε small enough not to flip any
  ordering, so analytic and numeric agree. This is the standard 3DGS treatment.
- **Tile assignment and screen radius.** Same reasoning — discrete, piecewise constant.
- **The +0.3 anti-aliasing filter.** Additive constant on Σ's diagonal: its derivative is the
  identity, so it changes nothing in the chain — the gradients w.r.t. the filtered Σ' pass
  straight through to the raw one.
- **The α ≤ 0.99 clamp.** In the clamped branch, ∂α/∂(anything) = 0 — the backward pass must
  recompute α, and if it hit the clamp, zero that splat's α-path gradients for that pixel
  (while still using α = 0.99 for transmittance reconstruction, matching forward exactly).
- **Skip thresholds (α < 1/255, T < 1e-4).** A skipped splat contributed nothing forward, so
  it gets nothing backward. The backward pass must replicate *exactly* the same skip/stop
  decisions as forward, or the reconstructed transmittances drift. This is why the forward
  pass must save, per pixel: the **final transmittance T_final** and the **contributor count**
  (how far into the tile's sorted range it actually got before the T < 1e-4 early-out).

**One thing that is NOT here and must not be forgotten later:** since the Gaussian is
*unnormalized* (no 1/(2π√det Σ') factor — 3DGS's choice, inherited here), Σ' affects α only
through d. There is no log-det term. If a normalized Gaussian is ever introduced, this whole
section changes.

## 1. Color: ∂L/∂c_i, and back to the SH coefficients

`c_i` enters C linearly in exactly one term:

```
∂L/∂c_i = (∂L/∂C) · T_i α_i        (per channel; summed over every pixel the splat touches)
```

Chain to the SH coefficients (channel ch, basis k), since `c_ch = 0.5 + Σ_k sh_{ch,k} b_k(v)`:

```
∂L/∂sh_{ch,k} = (∂L/∂c_ch) · b_k(v)
```

— the basis values `b_k` are the same ones the forward pass computed; nothing new needed.

## 2. Alpha: ∂L/∂α_i, the back-to-front recurrence

α_i appears in its own term **and** inside T_j for every splat j behind it (each such T_j
carries a factor (1 − α_i)). Differentiating `C = Σ_j T_j α_j c_j`:

```
∂C/∂α_i = T_i c_i − (1/(1−α_i)) · Σ_{j>i} T_j α_j c_j
        = T_i c_i − S_i/(1−α_i)  ,   where  S_i = color accumulated BEHIND splat i
```

**Worked check (2 splats):** C = α₁c₁ + (1−α₁)α₂c₂. Direct: ∂C/∂α₁ = c₁ − α₂c₂. Formula:
T₁c₁ − S₁/(1−α₁) = c₁ − (1−α₁)α₂c₂/(1−α₁) = c₁ − α₂c₂ ✓.

Computing this efficiently is why the backward pass walks each pixel's splat list
**back-to-front**, maintaining two running values:

```
initialize:  T_run = T_final (saved by forward),  S = 0
visit splat i (from the last contributor backward to the first):
    T_i  = T_run / (1 − α_i)          reconstruct transmittance BEFORE splat i
    use T_i and S in the gradient formulas for splat i
    S    ← S + T_i α_i c_i            splat i now becomes part of "behind" for i−1
    T_run ← T_i
```

The division is safe because the forward clamp guarantees (1 − α_i) ≥ 0.01.

Then `∂L/∂α_i = Σ_ch (∂L/∂C_ch) · (∂C_ch/∂α_i)`, summed over the three channels.

## 3. From α to the stored opacity logit, and to d

```
α = o · exp(−½ d)

∂α/∂o = exp(−½ d) = α/o
∂α/∂ℓ = (α/o) · o(1−o) = α(1−o)        (chain through o = sigmoid(ℓ), dσ/dℓ = σ(1−σ))
∂α/∂d = −½ α
```

`∂L/∂ℓ = (∂L/∂α)·α(1−o)` is the complete opacity gradient — the simplest full chain, which is
why the opacity gradient test lands first.

## 4. From d to the screen mean and the 2D covariance

Write `A = Σ'⁻¹` (symmetric 2×2) and `g = AΔ` (a 2-vector). Then `d = Δᵀ A Δ` gives:

```
∂d/∂Δ  = 2g                (standard quadratic-form derivative, A symmetric)
∂d/∂μ  = −2g               (Δ = p − μ; the pixel p is a constant)
∂d/∂Σ' = −g gᵀ             (full-matrix form, already symmetric)
```

The Σ' derivative comes from `d(A) = −A·dΣ'·A` (derivative of a matrix inverse), contracted
with Δ on both sides. **Packed-storage caution:** the kernel stores Σ' as (xx, xy, yy). In
that packed form the off-diagonal gradient is `∂d/∂(xy) = −2 g_x g_y` — the factor 2 because
the single stored value appears in *two* matrix slots. Diagonals: `−g_x²` and `−g_y²`. All
full-matrix chain formulas below use the symmetric full-matrix form; only at the final
write-out does the packed doubling apply.

## 5. Screen mean → position (path 1 of 3 into position)

`μ = (fx·x/z + cx, fy·y/z + cy)` where `t = (x, y, z)` is camera-space position. So
`∂μ/∂t = J₂`, the top 2×3 block of the same EWA Jacobian from the forward derivation:

```
J₂ = [ fx/z    0      −fx·x/z² ]
     [ 0      fy/z    −fy·y/z² ]

∂L/∂t  (this path)  = J₂ᵀ (∂L/∂μ)
∂L/∂p_world         = Wᵀ (∂L/∂t)       (t = W·p_world + translation, so ∂t/∂p_world = W)
```

## 6. 2D covariance → 3D covariance → scale and rotation

Let `U = J₂ W` (2×3 — row 2 of J is zero, so only this block ever matters), so the forward
projection is `Σ' = U Σ Uᵀ` and, with `G' = ∂L/∂Σ'` (symmetric full-matrix form):

```
∂L/∂Σ = Uᵀ G' U                        (3×3, symmetric)
```

Then `Σ = M Mᵀ` with `M = R S` (S = diag(s_x, s_y, s_z)); with `G = ∂L/∂Σ`:

```
∂L/∂M = (G + Gᵀ) M = 2 G M             (G symmetric)
```

(Derivation: dΣ = dM·Mᵀ + M·dMᵀ, take the Frobenius inner product with G and collect dM.)

Split M = R·S into its two factors:

```
∂L/∂s_k = (Rᵀ · 2GM)_{kk}              (S is diagonal — only the diagonal entries are real dof)
∂L/∂ℓ_k = s_k · ∂L/∂s_k                (chain through s = exp(ℓ) — the log-space storage)

∂L/∂R   = 2 G M Sᵀ = 2 G M S           (S diagonal)
```

**Rotation → quaternion.** With the normalized quaternion q̂ = (x, y, z, w) and the exact R
used in `preprocess.metal` / `core::covariance_3d`:

```
R = [ 1−2(y²+z²)   2(xy−wz)     2(xz+wy)   ]
    [ 2(xy+wz)     1−2(x²+z²)   2(yz−wx)   ]
    [ 2(xz−wy)     2(yz+wx)     1−2(x²+y²) ]

∂R/∂x = [ 0    2y   2z ]      ∂R/∂y = [ −4y   2x   2w ]
        [ 2y  −4x  −2w ]              [ 2x    0    2z ]
        [ 2z   2w  −4x ]              [ −2w   2z  −4y ]

∂R/∂z = [ −4z  −2w   2x ]     ∂R/∂w = [ 0   −2z   2y ]
        [ 2w   −4z   2y ]             [ 2z   0   −2x ]
        [ 2x    2y   0  ]             [ −2y  2x   0  ]

∂L/∂q̂_k = Σ_{ij} (∂L/∂R)_{ij} (∂R/∂q̂_k)_{ij}     (elementwise contraction, k ∈ {x,y,z,w})
```

Finally, chain through the on-read normalization q̂ = q/‖q‖ (the stored q is unnormalized):

```
∂q̂/∂q = (I − q̂ q̂ᵀ)/‖q‖      ⇒      ∂L/∂q = (I − q̂ q̂ᵀ)/‖q‖ · ∂L/∂q̂
```

This projection matters: it removes the component of the gradient along q itself (which only
changes the length, which normalization erases) — without it, the gradient test for rotation
fails for any stored quaternion that isn't already unit-length.

## 7. The J-dependence of Σ' → position (path 2 of 3 into position)

Σ' depends on position twice: through μ (path 1, above) and through **J₂ itself**, which is
evaluated at the splat's own camera-space position. Finite differences see both, so the
analytic gradient must include both or the position test fails.

From Σ' = U Σ Uᵀ with U = J₂ W:

```
∂L/∂U  = 2 G' U Σ            (2×3; same Frobenius argument as ∂L/∂M above)
∂L/∂J₂ = (∂L/∂U) Wᵀ          (2×3)
```

J₂'s entries depend on t = (x, y, z) as: `J₂[0][0] = fx/z`, `J₂[0][2] = −fx·x/z²`,
`J₂[1][1] = fy/z`, `J₂[1][2] = −fy·y/z²` (the rest are constant zero). Their derivatives:

```
∂J₂[0][0]/∂z = −fx/z²           ∂J₂[0][2]/∂x = −fx/z²        ∂J₂[0][2]/∂z = 2fx·x/z³
∂J₂[1][1]/∂z = −fy/z²           ∂J₂[1][2]/∂y = −fy/z²        ∂J₂[1][2]/∂z = 2fy·y/z³
```

So this path's contribution to ∂L/∂t, writing `H = ∂L/∂J₂`:

```
∂L/∂x += H[0][2]·(−fx/z²)
∂L/∂y += H[1][2]·(−fy/z²)
∂L/∂z += H[0][0]·(−fx/z²) + H[0][2]·(2fx·x/z³) + H[1][1]·(−fy/z²) + H[1][2]·(2fy·y/z³)
```

then through Wᵀ to world space, exactly as in path 1.

## 8. The SH view direction → position (path 3 of 3 into position)

Color depends on position through the view direction `v = u/‖u‖, u = p_world − cam`:

```
∂c_ch/∂v = Σ_k sh_{ch,k} · ∇b_k(v)
∂v/∂u    = (I − v vᵀ)/‖u‖                        (normalization Jacobian, same form as §6's)
∂L/∂p_world += (∂v/∂u)ᵀ · Σ_ch (∂L/∂c_ch)(∂c_ch/∂v)     (∂u/∂p_world = I)
```

Gradients of all 16 basis polynomials (constants as in `core/spherical_harmonics.hpp`;
each row is (∂/∂x, ∂/∂y, ∂/∂z)):

```
∇b0  = (0, 0, 0)
∇b1  = (0, −C1, 0)                     ∇b2  = (0, 0, C1)
∇b3  = (−C1, 0, 0)
∇b4  = C2[0]·(y, x, 0)                 ∇b5  = C2[1]·(0, z, y)
∇b6  = C2[2]·(−2x, −2y, 4z)            ∇b7  = C2[3]·(z, 0, x)
∇b8  = C2[4]·(2x, −2y, 0)
∇b9  = C3[0]·(6xy, 3x²−3y², 0)         ∇b10 = C3[1]·(yz, xz, xy)
∇b11 = C3[2]·(−2xy, 4z²−x²−3y², 8yz)   ∇b12 = C3[3]·(−6xz, −6yz, 6z²−3x²−3y²)
∇b13 = C3[4]·(4z²−3x²−y², −2xy, 8xz)   ∇b14 = C3[5]·(2xz, −2yz, x²−y²)
∇b15 = C3[6]·(3x²−3y², −6xy, 0)
```

(Each verified by expanding the polynomial and differentiating term-by-term — e.g.
b12 = C3[3]·(2z³ − 3x²z − 3y²z) ⇒ (−6xz, −6yz, 6z² − 3x² − 3y²) ✓.)

**Total position gradient = path 1 + path 2 + path 3.** Omitting either of the last two
produces a gradient that is *almost* right — the exact failure mode the finite-difference
test exists to catch, and why it compares against all-paths-summed, not path 1 alone.

## 9. The loss side: ∂L/∂C

The backward kernel is **loss-agnostic**: it takes the per-pixel gradient image ∂L/∂C as an
input buffer and never knows which loss produced it. That keeps the hardest kernel in the
project decoupled from the (CPU-side, swappable) loss.

For L1, `L = (1/N) Σ |C − target|` (N = pixel count × 3 channels):

```
∂L1/∂C = sign(C − target)/N       (subgradient 0 at exact equality — measure-zero, harmless)
```

The D-SSIM gradient (through every window's means/variances/covariance) is genuinely more
involved and is **deliberately deferred to Phase 6**, where the training loop first runs —
the gradient tests in 5.3–5.6 exercise the render-side chain with L1 (or any synthetic
∂L/∂C), which covers every kernel path; which loss feeds the kernel changes nothing about
what the kernel must compute.

## 10. Accumulation and verification

- One pixel produces gradient contributions for every splat that contributed to it; a splat
  touches many pixels across many tiles. All contributions **atomically add** into per-splat
  gradient buffers (`atomic_float fetch_add`, availability verified on-device at startup).
  Float addition is not associative ⇒ results are not bitwise-reproducible across runs ⇒
  every gradient test uses tolerances, never exact equality.
- Central-difference recipe per parameter θ: `(L(θ+ε) − L(θ−ε))/2ε` with ε scaled to the
  parameter (position in world units needs a different ε than a logit), on a <100-splat,
  64×64 synthetic scene. Assert relative error < 1e-3 with an absolute floor to avoid
  dividing by near-zero gradients.
