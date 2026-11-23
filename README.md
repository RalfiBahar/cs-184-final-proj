# Silver Lining — Volumetric Renderer

A C++ CS184 final project extending our HW3 path tracer with participating media.

## Integrated features

| Part | Implementation |
|---|---|
| A | Bounded media, homogeneous absorption/scattering, mixed surface/volume paths, scene configuration |
| B | Henyey–Greenstein phase evaluation/sampling and direct volume lighting |
| C | Seeded procedural density grids, trilinear interpolation, majorants and delta-tracked collisions |
| D | Analytic homogeneous attenuation and ratio-tracked heterogeneous shadow attenuation |

The application selects the configured medium and phase. Cloud shadow connections use ratio tracking;
opaque visibility is checked separately. Surface and volume direct lighting share medium attenuation.
Omitting `-v` preserves the surface-only renderer.

## Build and test

Verified toolchain: Windows, Visual Studio 2026/MSVC, CMake, Ninja, and vcpkg dependencies.
From a Visual Studio developer terminal at the repository root:

```powershell
cmake -S renderer -B renderer/out/build/x64-Release -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DBUILD_VOLUME_TESTS=ON
cmake --build renderer/out/build/x64-Release --parallel 6
ctest --test-dir renderer/out/build/x64-Release --output-on-failure
```

Replace the vcpkg path. OpenGL and Freetype are required; CGL/GLFW/GLEW/ImGui are bundled.
`BUILD_VOLUME_TESTS=ON` registers Part A regressions, B phase/lighting and integrator tests,
combined A/B/C/D tests, and D's evaluation with two seeds.

## Render comparisons

```powershell
./renderer/scripts/render_integrated.ps1 -Samples 64 -Size 128
```

This renders baseline, vacuum, absorption, homogeneous fog, isotropic cloud and HG cloud
using one camera and common settings. Outputs are under `renderer/renders/integrated/`.
Pass `-Executable` to use a different build or `-Threads 4` for a multithreaded smoke render.
Use one thread for repeatable comparisons; legacy light/BSDF streams can depend on scheduling.

## Volume settings

```text
bounds_min -1 0.1 -0.95
bounds_max 1 1.7 0.95
cloud_resolution 48
density 1
extinction_scale 5
albedo 0.93
phase_g 0.5
seed 184
```

- Omit `cloud_resolution` for a homogeneous medium; include it for a procedural cloud.
- `phase_g` must be strictly between -1 and 1. It defaults to 0 (isotropic); positive values favor forward scattering.
- Extinction is density times extinction scale. Albedo is the scattering fraction in [0,1].
- Bounds use internal world coordinates. The volume boundary is not opaque geometry.
- Unknown, duplicate, malformed and invalid settings are rejected.
- `scenes/volume/cornell_cloud.vol` and `cornell_cloud_isotropic.vol` differ only in phase anisotropy.

To open the viewer, run from `renderer`:

```powershell
./out/build/x64-Release/pathtracer.exe -s 64 -l 4 -t 1 -m 6 `
  -c scenes/volume/cornell_demo.cam -v scenes/volume/cornell_cloud.vol `
  scenes/volume/cornell_demo.dae
```

Press **R** to render. Add `-f cloud.png` for a file render.

## Code map

- `renderer/src/pathtracer/volume_integrator.cpp`: mixed radiance loop and surface-shadow attenuation.
- `renderer/src/volume/medium.h/.cpp`: shared contract and sidecar parser.
- `renderer/src/volume/hg_phase.h`: HG scattering.
- `renderer/src/volume/direct_lighting.h`: volume lighting and opaque visibility.
- `renderer/src/volume/cloud_medium.h`: procedural grid, interpolation, delta tracking and ratio-shadow dispatch.
- `renderer/src/volume/transmittance.h/.cpp`: attenuation estimators and reference quadrature.
- `renderer/tests/combined_volume_tests.cpp`: tests spanning all four modules.
- `renderer/docs/INTEGRATION.md`: integration changes, tests and limitations.

## Scope

One bounded, nonemissive volume with scalar extinction/albedo; CPU rendering only.
Overlapping media, spectral extinction, animated clouds and polished god-ray scenes remain future work.
Unfinished inherited advanced materials/light types are outside the tested diffuse/emissive baseline.
A passing test suite is evidence for its fixtures, not exhaustive physical or race-detector validation.
