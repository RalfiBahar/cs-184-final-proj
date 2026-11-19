# Silver Lining — Volumetric Renderer

A C++ path tracer for our CS184 final project. This repository extends our HW3 renderer with participating media.

## Current Progress: Part A

Implemented:

- Bounded homogeneous fog with vacuum outside the volume
- Density, extinction, and scattering albedo configuration
- Exponential free-flight collision sampling
- Isotropic scattering
- Mixed surface and volume path tracing
- Direct lighting with medium attenuation and opaque shadowing
- Analytic homogeneous transmittance
- Configurable volume sidecars through the `-v` option
- Automated correctness and integration tests

The original surface-only renderer remains available by omitting `-v`.

## Repository Structure

```text
renderer/
├── CGL/                         # Supporting graphics library and dependencies
├── dae/                         # Existing scenes
├── scenes/volume/               # Part A demo scene, camera, and medium settings
├── scripts/render_part_a.ps1    # Repeatable demo renders
├── src/
│   ├── pathtracer/
│   │   └── volume_integrator.cpp # Main volume rendering loop
│   └── volume/
│       ├── medium.h             # Shared medium interface
│       ├── medium.cpp           # Homogeneous medium and configuration parser
│       └── phase.h              # Phase interface and isotropic implementation
├── tests/volume_tests.cpp       # Correctness and integration tests
└── docs/
    ├── PART_A.md                # Implementation details and team interfaces
    └── PART_A_VERIFICATION.md   # Verification results and limitations
```

## Build

The implementation was verified on Windows with Visual Studio 2026, MSVC, CMake, Ninja, and dependencies supplied through vcpkg.

From a Visual Studio developer terminal at the repository root:

```powershell
cmake -S renderer -B renderer/out/build/x64-Release -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DBUILD_VOLUME_TESTS=ON

cmake --build renderer/out/build/x64-Release --parallel 6
```

Replace the vcpkg path with your local installation. The build requires OpenGL, Freetype, and the dependencies specified in the CMake files.

## Run the Demo

From the repository root:

```powershell
.\renderer\scripts\render_part_a.ps1 -Mode fog -Samples 64 -Size 128
```

Available modes:

| Mode | Description |
|------|-------------|
| `baseline` | Original surface-only rendering |
| `vacuum` | Volume integrator with zero density |
| `absorption` | Homogeneous absorption without scattering |
| `fog` | Homogeneous scattering and absorption |

Generated images are saved to:

```text
renderer/renders/part-a/
```

To launch the interactive viewer, run from the `renderer` directory:

```powershell
.\out\build\x64-Release\pathtracer.exe `
  -s 64 -l 4 -t 1 -m 6 `
  -c scenes/volume/cornell_demo.cam `
  -v scenes/volume/cornell_fog.vol `
  scenes/volume/cornell_demo.dae
```

Press **R** to render the participating medium.

## Medium Configuration

Example `.vol` file:

```text
bounds_min -1 0 -1
bounds_max 1 1.5 1
density 1
extinction_scale 0.65
albedo 0.9
seed 184
```

- Bounds use the renderer’s internal world coordinates.
- Total extinction is `density × extinction_scale`.
- Albedo is the fraction of extinction assigned to scattering, between `0` and `1`.
- Zero density produces vacuum.
- Invalid, missing, duplicate, or unknown settings are rejected.

The volume boundary is a mathematical region, not an opaque surface.

## Tests and Verification

Run:

```powershell
ctest --test-dir renderer/out/build/x64-Release --output-on-failure
```

Release and RelWithDebInfo builds passed locally. The test executable passed **953,738 checks**, including repeated Monte Carlo trials.

Coverage includes:

- Volume bounds and nearest-surface clipping
- Cameras inside and outside the medium
- Vacuum and analytic absorption
- Free-flight collision statistics
- Isotropic phase sampling
- Analytic single scattering
- Surface and volume shadow attenuation
- Opaque blockers
- Area-light sampling against a rectangle solid-angle reference
- Emitter visibility and duplicate-contribution suppression
- Path-depth behavior
- Invalid configuration handling

Render checks covered baseline, vacuum, absorption, fog, an inside-camera view, and a four-thread render. Repeated single-thread fog renders produced identical PNG hashes.

These are focused Part A checks, not an exhaustive validation of the entire inherited renderer.

## Team Integration

Part A provides the shared medium foundation and rendering interfaces.

Remaining integration work includes:

- **B:** Henyey–Greenstein phase function and lighting contributions
- **C:** Heterogeneous density grids and delta tracking
- **D:** Ratio-tracked transmittance and evaluation tooling

See `renderer/docs/PART_A.md` for interface conventions, ownership boundaries, and integration details.

## Limitations

The current implementation supports one bounded, nonemissive homogeneous medium with scalar extinction and albedo.

Not yet implemented:

- HG anisotropy
- Heterogeneous clouds and delta tracking
- Ratio tracking
- Overlapping or nested media
- Colored extinction, animation, or GPU acceleration

Some inherited advanced materials, environment lighting, and light types remain incomplete. Current validation targets the diffuse/emissive Part A baseline. No full white-furnace validation or race-detector pass is claimed.
