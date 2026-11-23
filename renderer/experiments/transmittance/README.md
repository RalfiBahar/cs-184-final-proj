# Transmittance evaluation (Owner D)

Ratio-tracking transmittance for the Silver Lining volume renderer, plus an
experiment that checks it against exact answers.

## What is here

| File | Purpose |
| --- | --- |
| `../../src/volume/transmittance.h/.cpp` | `ratio_tracking_transmittance()`, `analytic_homogeneous_transmittance()`, `optical_depth_quadrature()` (Simpson reference). Attenuation only: no opaque geometry. |
| `test_media.h` | Test-only `LinearRampMedium`: density ramps linearly along z, so optical depth along any straight ray has a closed form. Also implements delta tracking so escape probability can be compared. |
| `eval_transmittance.cpp` | Draws N independent estimates per fixture, reports mean, standard error, exact value, z-score; writes `transmittance.csv` and `timing.csv`. Exits nonzero on failure. |
| `plot_transmittance.py` | Plot of estimate vs exact (left) and z-scores (right). |
| `run.sh` | Builds, runs, records machine/commit info, makes the plot. |
| `results/` | Output of the last run (CSVs, plot, `run_info.txt`, `console.txt`). |

## Run

Needs a C++17 compiler and `python3` with `matplotlib`. It does not need
OpenGL, GLFW, vcpkg, or a full renderer build (it compiles `medium.cpp` and
`transmittance.cpp` directly and skips `bbox.cpp`, which pulls in GLEW).

```bash
renderer/experiments/transmittance/run.sh            # 200000 trials, seed 184
renderer/experiments/transmittance/run.sh 1000000 184
```

## Convention (matches the team contract)

- `sigma_t(x) = density(x) * extinctionScale`, scalar, wavelength independent.
- Ray directions are unit length; the segment is in absolute world distance and
  is clipped by the caller (`Medium::clip`) to the volume and the nearest surface.
- Ratio tracking draws candidates at rate `majorant_sigma_t()` and multiplies the
  estimate by `1 - sigma_t(x) / majorant` at each. Every estimate is in `[0, 1]`.
- If `sigma_t(x)` exceeds the majorant, it **throws** instead of clamping.
- Vacuum (zero majorant) or an empty segment returns exactly 1.

## Fixtures

Zero density; zero-length segment; homogeneous slabs with tau = 0.25, 1, 4;
camera inside the volume; an oblique ray; a surface blocker inside the volume;
and linear-ramp media (up, down, oblique, steep, blocker). Two estimators are
compared against the same exact answer: ratio tracking (mean of estimates) and
the escape fraction of delta tracking (equal to exp(-tau) for a valid majorant).
A deterministic Simpson quadrature check confirms the "exact" optical depths
independently of the closed forms. A deliberately invalid majorant checks that
violations are reported.

## Reading the results

A fixture passes when `|mean - exact| <= 5 standard errors` (and, for ratio
tracking, all estimates lie in `[0, 1]`). When the standard error is exactly 0
(vacuum, zero length) the match must be exact. This is a statistical check:
with many comparisons an occasional |z| near 2.5-3 is expected, and one noisy
run is neither a proof of correctness nor a failure. Re-run with another seed
(`run.sh 1000000 2`) before reading anything into a single outlier. Error bars in
the left plot are smaller than the markers at these trial counts, so the
right-hand z-score panel is the more informative view.

## Limits

- Scalar (non-spectral) extinction only; per-channel extinction is future work.
- The evaluation fixtures are hand-built. The combined CMake tests additionally exercise
  ratio tracking through `CloudMedium` and the actual surface/volume integrator.
- Timings are single-thread microbenchmarks of the estimators on one machine,
  not render times (see `results/run_info.txt` for the machine and commit).
- This module does not do opaque-surface visibility; that stays with the lighting helper.
