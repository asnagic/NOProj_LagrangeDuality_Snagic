# Lagrange Duality Explorer

Numerical Optimization 2025/26 — **Project 18: Lagrange Duality: Dual Function Construction,
Strong Duality, and Sensitivity Analysis**, built on the natID framework.

## What it does

Six tabs, all driven by one model — edit anything anywhere and the whole application
re-solves and repaints.

| Tab | Content |
|---|---|
| 1. Primal QP | Editable convex QP. Objective contours, the feasible polyhedron, the unconstrained minimiser and `x*`. Active constraints are drawn heavy, inactive ones dashed. |
| 2. Dual function | `g(λ)` as a curve (m = 1) or a filled contour map over the non-negative quadrant (m ≥ 2), with `λ*` marked and `p*` as a reference line. |
| 3. Sensitivity | `p*(u)` recomputed exactly for a perturbed constraint level, against the shadow-price tangent `p* − λ*ᵢ u`. |
| 4. Resource allocation | Water-filling: `max Σ wᵢ log(1 + xᵢ/αᵢ)` s.t. `Σ xᵢ ≤ B`. One multiplier, so the dual is a curve. Second plot shows floors, water level and allocations. |
| 5. Duality gap | A non-convex instance where strong duality fails: `f₀`, the dual `g(λ)`, and the perturbation function `p(u)` with its convex envelope — the gap is the vertical distance at `u = 0`. |
| 6. Theory and checks | The derivations, plus every claim checked against the numbers currently on screen. |

## The three instances

**A. Convex QP** — `min ½x'Px + q'x  s.t. Ax ≤ b`, `P ≻ 0`.
The inner minimisation is closed form, `x(λ) = −P⁻¹(q + A'λ)`, giving

```
g(λ) = −½ (q + A'λ)' P⁻¹ (q + A'λ) − b'λ
```

The dual is maximised over `λ ≥ 0` by exact cyclic coordinate ascent. The primal is solved
a **second, independent time** by a logarithmic barrier method (Newton with a backtracking
line search and a squared-hinge phase I), so `p*` and `d*` are two genuinely separate
computations that are then compared — not one number printed twice.

**B. Resource allocation** — only the budget is dualised, leaving `x ≥ 0` in the domain, so
there is exactly one multiplier. The Lagrangian separates and gives the water-filling rule
`xᵢ(λ) = max(0, wᵢ/λ − αᵢ)`; `λ*` is found by bisection on `Σ xᵢ(λ) = B`.

**C. Non-convex** — `min x⁴ + c₂x² + c₁x  s.t. x ≥ xMin`. With `c₂ < 0` this is a double
well; when the global minimum falls outside the feasible region, `d* < p*`. Defaults give
`p* = −7.289`, `d* = −9.000`, gap `1.711`.

## Verified numbers (default data)

```
QP:          x* = (0.5, 1.5)   λ* = (0.5, 0)
             p* = d* = −4.25,  gap 0.0e+00
             log-barrier primal agrees to 1e−10
             complementary slackness products = 0 exactly
             dp*/du₁ = −0.5 = −λ₁*,  dp*/du₂ = 0 = −λ₂*
Allocation:  λ* = 0.714285714,  x* = (3.2, 2.3, 0, 0.5),  gap 0.0e+00
             dp*/dB = −0.714285714 = −λ*
Non-convex:  p* = −7.2893,  d* = −9.0 at λ* = 1,  gap = 1.7107
             convex envelope of p at u=0 = −8.9994, matching d* to grid resolution
```

## Building

Requires the natID SDK at `~/natID.SDK` and CMake.

```bash
cmake -S ~/natID.Projects/LagrangeDuality -B ~/natID.RAMDisk/Out/LagrangeDuality \
      -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build ~/natID.RAMDisk/Out/LagrangeDuality -j8
```

Run with the resource path pointing at the project folder:

```bash
~/natID.RAMDisk/Out/lagrangeDualitySol/Debug/lagrangeDuality.app/Contents/MacOS/lagrangeDuality \
    -devResPath=$HOME/natID.Projects/LagrangeDuality
```

### Exporting the figures

`-export=<folder>` walks every tab, writes each plot to PDF and quits — this is how the
figures for the written report are produced. Export mode opens a larger window than normal
use, because each figure is written at its on-screen canvas size.

```bash
mkdir -p ~/natID.Projects/LagrangeDuality/figures
~/natID.RAMDisk/Out/lagrangeDualitySol/Debug/lagrangeDuality.app/Contents/MacOS/lagrangeDuality \
    -devResPath=$HOME/natID.Projects/LagrangeDuality \
    -export=$HOME/natID.Projects/LagrangeDuality/figures
```

### macOS Gatekeeper — read this before running anything from the SDK

The SDK dylibs are ad-hoc signed. If they still carry a download quarantine flag, macOS
puts up an "allow this library?" prompt, blocks the load if nobody answers — **and deletes
the file**. That is how `mainUtils.dylib` and `mainUtilsD.dylib` were lost here; they were
restored from `bin_macOS_arm64_20260710.7z`. Clear the flag once after installing or
updating the SDK, before the first run:

```bash
xattr -dr com.apple.quarantine ~/natID.SDK
```

A copy of the pre-existing (2026-05-17) libraries is kept at `~/natID.SDK/bin/lib.backup-may17`.
The two restored files are from the July build; they link and run correctly against the rest
of the May set.

## Layout

```
figures/                  eight exported PDF figures, regenerate with -export
src/
  main.cpp, Application.h, MainWindow.h, MainView.h
  core/
    LinAlg.h            dense LU, Cholesky, vector helpers
    QPProblem.h         dual coordinate ascent + independent log-barrier primal
    AllocationProblem.h water filling
    NonconvexProblem.h  grid + golden section, convex envelope by monotone chain
    AppModel.h          the three instances, half-plane clipping for the feasible polygon
  views/
    PlotCanvas.h        axes, ticks, series, markers, filled contour fields, legend
    ViewQP.h  ViewDual.h  ViewSensitivity.h  ViewAllocation.h  ViewGap.h  ViewTheory.h
res/
  DevRes.xml, main.xml, tr/{EN,BA}/main.xml
```

`core/` has no GUI dependency and compiles standalone, which is how the numerics were
tested before any drawing code existed.
