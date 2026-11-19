# Part A: homogeneous participating media

This implementation extends the team's current HW3 code. It provides one bounded,
nonemissive homogeneous medium, scalar extinction/albedo, analog free-flight
sampling with survival weighting, isotropic scattering, direct-volume light
sampling, and attenuated surface and volume shadow connections. It is CPU code.

## Build and run

Use the existing Visual Studio x64-Release configuration (it supplies your vcpkg
toolchain). Build All as before. Omit `-v` to keep the HW3 integrator.

From the renderer directory in PowerShell, after building:

```powershell
.\out\build\x64-Release\pathtracer.exe -s 64 -l 4 -t 1 -m 6 -a 0 0 -r 256 256 -c scenes/volume/cornell_demo.cam -v scenes/volume/cornell_fog.vol -f fog.png scenes/volume/cornell_demo.dae
```

Replace cornell_fog.vol with cornell_absorption.vol or vacuum.vol to compare.
For interactive viewing, omit `-f fog.png`; the viewer initially shows surfaces,
so press R to path trace the participating medium. The no-argument launcher's
GUI has no medium controls yet; use `-v` in a terminal or launch arguments.

The demo is a copy of CBempty with reduced point-light intensity and a fixed
camera, to avoid clipping the inherited output to white. The existing scene
files are unchanged. COLLADA input is converted to internal Y-up coordinates;
the demo volume spans x=[-1,1], y=[0,1.5], z=[-1,1].

The text sidecar uses internal world coordinates. All four keys bounds_min, bounds_max,
extinction_scale, albedo are required. density defaults to 1 and seed to 184.
Unknown, missing, duplicate, malformed, or invalid values are rejected.
sigma_t = density * extinction_scale; sigma_s = albedo * sigma_t.
The box is a mathematical volume boundary, not geometry or an opaque material.

The volume integrator uses accumulated bounces and explicit light sampling.
It rejects `-o 0`, `-H`, and zero light samples, which would require a separate
estimator. The legacy no-volume integrator still supports its previous modes.
Depth counts real surface/volume scattering vertices; at depth 1, direct light
is evaluated at the first vertex. Depth 0 still allows attenuated camera-visible
emission. A finite bounce cap truncates higher-order transport.

## Team contract (actual committed header candidates)

- `src/volume/medium.h`: `Volume::Medium`, `Interval`, `Collision`, `RNG`, `Settings`.
- `Medium::clip(ray, endpoint, interval)` clips against ray.min_t/ray.max_t,
  the nearest surface or light endpoint, and the medium bounds.
- Rays supplied to this interface have UNIT directions. Interval begin/end and
  Collision.t are absolute parameters on that ray, in world-distance units.
- `sample_collision` returns a real TOTAL-EXTINCTION collision or escape.
  It never returns null candidates as real events and never changes throughput.
- The integrator multiplies throughput by `albedo(position)` at a real collision.
  Escape leaves throughput unchanged. Do not multiply continuation by Tr again.
- `transmittance` estimates attenuation on an already clipped shadow interval;
  it must not query opaque geometry. The caller checks visibility separately.
- Coefficients are wavelength-independent scalars; RGB incident radiance works.
- A medium is immutable during rendering. `Volume::RNG&` is path-local.

### B: phase and lighting

Implement HG by deriving `Volume::PhaseFunction` in `src/volume/phase.h` or your
own phase header. Set `PathTracer::phase` to the implementation before rendering.
`evaluate(incoming,outgoing)` uses travel directions: incoming points INTO the
event along the ray, outgoing points toward the next vertex/light. Positive g
means dot(incoming,outgoing) near +1. PBRT's both-away convention differs.
`sample` returns direction, phase value, and solid-angle PDF. The integrator
applies value/pdf; isotropic sampling defaults to value=pdf=1/(4*pi).

The temporary isotropic NEE helpers are
`PathTracer::volume_direct_lighting` and `surface_direct_lighting_volume`, in
`src/pathtracer/volume_integrator.cpp`. Coordinate changes there with A. Each
light is sampled individually; delta lights get one sample, area lights get
ns_area_light samples. There is no additional light-selection PDF. Volume NEE
has no surface cosine. Surface NEE retains the BSDF*cosine factor.

NEE-only emitter policy: camera and paths following delta surfaces can directly
see emission. After regular phase/diffuse scattering, direct emitter/environment
hits are suppressed because NEE estimates them. Emitters must be represented by
the scene light sampler; arbitrary unregistered emissive meshes need registration
or an MIS/alternative estimator before being supported correctly.

### C: density and collision sampling

Derive `Volume::Medium`; supply grid density, sigma_t, albedo, conservative
majorant, and delta-tracked `sample_collision`. Do not add volume bounds to the
BVH. Return candidate position/t only for accepted real extinction events.
No throughput factor or PDF is needed in Collision under this scalar analog
contract. Null collisions do not spend scattering depth. Reject invalid
majorants instead of silently clamping acceptance probabilities.

### D: shadow attenuation and evaluation

Implement ratio tracking via the heterogeneous medium's virtual `transmittance`
or delegate to your helper. `PathTracer::medium_transmittance` is the one common
call site for BOTH volume and surface direct-light connections. It clips first;
the shadow caller handles blockers. Keep random shadow estimates independent
of continuation collision samples (successive RNG draws are fine).

### A: configuration and integration

`AppConfig::volume_settings` flows through `Application` and
`RaytracedRenderer::set_volume` to `PathTracer::medium` and volume_seed.
`PathTracer::clear()` deliberately preserves these settings when rerendering.
`load_settings` currently constructs HomogeneousMedium only. When C's medium
works, extend the sidecar loader by agreement rather than duplicating app setup.

## Verification

Configure CMake with `-DBUILD_VOLUME_TESTS=ON`, then build `volume_tests` and run
`ctest --test-dir <build directory> --output-on-failure` or run volume_tests.exe.
Tests run without a viewer and use the actual path integrator for radiance.

Tests include: parallel/on-face box rays, cameras inside/outside, surface endpoint
clipping, vacuum and zero-length intervals, invalid coefficients/bounds/config,
free-flight escape probability against exp(-sigma_t*d), isotropic sampling,
emissive surfaces inside/in front of fog, analytic single scattering, surface
shadow transmittance, and opaque blocking. Statistical tests use five standard
errors plus a small numerical tolerance, rather than demanding exact noisy values.

Additional acceptance regressions cover inside-camera absorption against the
analytic solution, multi-bounce phase and nondelta-surface emitter-hit suppression,
preserved delta-surface emitter visibility, depth-zero and
higher-depth behavior, malformed/duplicate configuration values, nonfinite
coefficients, RGB radiance validity, and an actual area light checked against
the independently calculated solid angle of a rectangle. The phase emitter
test deliberately uses a deterministic test phase and no registered lights to
isolate the emitter-hit policy; it is not a white-furnace validation.

Local verification also rendered baseline/vacuum/absorption/fog at 128x128,
64 spp, depth 6, one thread; an inside-camera view and four-thread smoke render
used 32 spp. Repeating the single-thread fog command produced the same PNG hash.
Unsupported volume CLI modes and a missing settings file returned failure.

Two issues were repaired during expanded verification: area-light PDF conversion
now uses the normalized direction (the rectangle reference detected excess
energy), and MSVC debug configurations embed compiler symbols with /Z7 to avoid
the C1902 compiler PDB-manager failure while retaining linked executable PDBs.
The area-light correction also applies to the legacy surface renderer.

The existing random_util engine is made thread_local to remove concurrent access
by surface/light samplers. Fixed volume seeds reproduce volume draws per pixel,
but scheduling still affects legacy BSDF/light random streams in multi-threaded
runs. Use `-t 1 -a 0 0` for reproducible comparison commands. Renderer BVH counters
are inherited non-atomic diagnostics; their multi-threaded totals are not precise.

## Scope and limitations

Not implemented in Part A: HG anisotropy, heterogeneous grids/delta tracking,
ratio tracking, dispersion, colored extinction, animation, denoising, GPU work,
or overlapping/nested media. Default phase and analytic shadow Tr are explicit
temporary implementations for integration, not claims of teammates' completed
features. Small ray epsilons introduce the usual tiny boundary error. No full
white-furnace validation is claimed. Advanced HW3 material/environment code may
still contain the starter's incomplete routines; the intended baseline is the
team's diffuse/emissive HW3-1 scenes.

References: PBRT 4e Volume Scattering and Volume Scattering Integrators:
https://pbr-book.org/4ed/Volume_Scattering
https://pbr-book.org/4ed/Light_Transport_II_Volume_Rendering/Volume_Scattering_Integrators
