# Part A verification

Verified November 18, 2026 on Windows with MSVC 19.51 and the existing vcpkg dependencies.

## Builds and tests

- `renderer/out/build/x64-Release`: RelWithDebInfo, build passed, executable PDBs produced.
- `renderer/out/build/part-a-release`: clean Release build passed.
- CTest passed in both configurations; 953738 checks including repeated Monte Carlo trials.
- Acceptance tests cover bounded medium clipping, inside/outside rays, nearest surfaces, vacuum,
  analytic extinction, free-flight statistics, isotropic phase, single scattering, surface shadow
  attenuation, opaque blockers, inside-camera absorption, depth behavior, area-light PDF correctness,
  phase/nondelta emitter suppression, delta emitter visibility, and malformed/nonfinite settings.
- The area-light rectangle reference failed before the PDF fix (0.13746 versus 0.0640942) and passes after it.

From a Visual Studio developer shell, reproduce the default build/test:

```powershell
cmake -S renderer -B renderer/out/build/x64-Release -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_VOLUME_TESTS=ON
cmake --build renderer/out/build/x64-Release --parallel 6
ctest --test-dir renderer/out/build/x64-Release --output-on-failure
```

## Renderer smoke checks

- Baseline, vacuum, absorption, fog: 128x128, 64 spp, four light samples, depth 6, one thread.
- Inside-camera fog and four-thread fog: 128x128, 32 spp; completed and visually checked.
- Repeating the one-thread fog command produced the same PNG SHA256.
- Vacuum/baseline image channel means differ by less than 0.1 out of 255. This is a smoke comparison,
  not a linear-radiance convergence or statistical-equivalence proof.
- Volume CLI rejected unsupported hemisphere/bounce-isolation modes, zero light samples, and missing settings.
- Output images are under `renderer/renders/part-a/` (ignored generated files).

```powershell
./renderer/scripts/render_part_a.ps1 -Mode fog -Samples 64 -Size 128
```

## Fixes made during verification

1. Corrected the actual area-light solid-angle PDF to use a normalized direction. This fixes excess
   energy in volume and surface direct lighting. Coincident light samples now return zero safely.
2. MSVC compiler debug symbols use /Z7 instead of /Zi, avoiding compiler PDB-server C1902 failures.
   Linker-generated executable PDBs remain available.
3. Expanded acceptance regressions and checked all RGB channels for finite nonnegative radiance.

## Remaining integration scope

This verifies the homogeneous Part A implementation with isotropic phase and analytic transmittance.
HG, heterogeneous density/delta tracking, and ratio tracking remain B/C/D integration work.
A deterministic test phase/BSDF isolates emitter policy; no full white-furnace test is claimed.
Four-thread completion is a smoke check, not a race-detector pass; inherited BVH diagnostic counters
remain non-atomic. Existing unfinished advanced materials/environment/light types are outside this baseline.
No commit, push, submission, or team contract approval was performed.
