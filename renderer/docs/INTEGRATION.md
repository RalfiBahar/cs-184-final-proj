# A/B/C/D integration

## Connections

`load_settings` constructs the homogeneous or procedural medium and the configured phase.
`phase_g` is optional (default 0); HG is selected for nonzero values in (-1,1).
`Application` calls `RaytracedRenderer::set_volume`, which delegates to `PathTracer::set_volume`.
The configured medium, phase and seed survive `PathTracer::clear()` for rerenders.

The radiance loop clips against the nearest opaque surface, samples a real extinction collision,
weights it by albedo, evaluates direct lighting and samples the configured phase.
Null collisions spend no scattering depth. Escape is not multiplied by transmittance again.
Both surface and volume shadow connections call `medium_transmittance`.
Homogeneous media use analytic attenuation; `CloudMedium::transmittance` delegates to Part D's ratio tracker.
Opaque visibility remains separate from attenuation, and emitter suppression avoids double-counting NEE.

## Build and regression fixes

- Registered transmittance.cpp in the production renderer.
- BUILD_VOLUME_TESTS now includes A, B, combined and D test targets.
- Shared test support objects preserve the renderer's AVX-dependent vector ABI and avoid repeated support compilation.
- Standalone B build scripts also compile transmittance.cpp, now needed by cloud medium construction.
- Cloud majorants include a conservative rounding margin; delta and ratio tracking reject bound violations.
- Ratio tracking rejects nonfinite/negative majorants, never accepts negative weights, and advances past zero-length draws.
- BVH diagnostic counters are initialized atomics with relaxed increments; reporting loads their values explicitly.

## Tests

The combined test exercises real production modules and the actual radiance loop:

- Sidecar parsing, HG selection, default isotropy, application setup and rerender retention.
- Grid boundary/interpolation behavior, procedural seed reproducibility and majorants.
- Constant/ramp cloud collision and ratio-shadow statistics against exact optical depths, using two seeds.
- Same-seed comparison proving that cloud virtual transmittance uses the ratio tracker.
- Varying cloud + HG + ratio tracking + the actual integrator against analytic single scattering at g=0, 0.5, 0.8.
- Surface NEE through a cloud and opaque blockage.
- Invalid phase values, nonfinite coefficients and tiny majorant violations.
- Parallel visibility queries with exact atomic diagnostic counts.

CTest also runs the existing A regression, B HG/lighting, B synthetic render/integrator, and D's 13-fixture evaluations.
The combined test exports `cloud_density_slice.pgm` in its working directory.
Use `scripts/render_integrated.ps1` for equal-settings image comparisons.

## Limits

Local verification covers Windows/MSVC; the updated macOS/Linux standalone script link lines require verification there.
Multithreaded smoke tests and atomic counter checks are not a full race-detector pass.
Legacy light/BSDF RNG streams can still differ with scheduling, so use one thread for reproducible image comparisons.
The single-volume scalar-extinction model and inherited advanced-material/light limitations remain.
Historic Part A/B/D handoff documents describe their original snapshots; this document records their subsequent integration.
