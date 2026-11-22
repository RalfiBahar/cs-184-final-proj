#ifndef SILVER_LINING_TRANSMITTANCE_H
#define SILVER_LINING_TRANSMITTANCE_H

#include "volume/medium.h"
#include <cstdint>

namespace CGL { namespace Volume {

// Owner D: attenuation-only helpers. None of these intersect opaque geometry;
// the caller clips `segment` to the nearest surface/light endpoint first
// (Medium::clip), exactly as for Medium::sample_collision.
//
// Convention (see the team contract): sigma_t(x) = density(x) * extinctionScale,
// scalar and wavelength independent. `ray.d` must be a UNIT vector so segment
// bounds are absolute world distances.

struct TransmittanceStats {
  uint64_t candidates = 0;  // majorant candidates generated (Poisson process)
};

// exp(-sigma_t * distance). Returns 1 for sigma_t == 0 or distance <= 0.
double analytic_homogeneous_transmittance(double sigma_t, double distance);

// Unbiased ratio-tracking estimate of exp(-integral of sigma_t along segment).
// Candidate points are drawn at rate majorant_sigma_t(); each multiplies the
// estimate by (1 - sigma_t(x) / majorant). Every estimate lies in [0, 1].
// Throws std::runtime_error if sigma_t(x) exceeds the majorant (invalid bound):
// the ratio is never clamped, so a broken majorant is reported, not hidden.
// Empty segment or zero majorant returns exactly 1 (vacuum).
double ratio_tracking_transmittance(const Medium& medium, const Ray& ray,
                                    Interval segment, RNG& rng,
                                    TransmittanceStats* stats = nullptr);

// Deterministic reference: composite Simpson integration of sigma_t along the
// segment, returned as optical depth tau. Independent of the stochastic
// estimators; `intervals` must be even and >= 2 (rounded up to even).
double optical_depth_quadrature(const Medium& medium, const Ray& ray,
                                Interval segment, int intervals = 4096);

} } // namespace CGL::Volume
#endif
