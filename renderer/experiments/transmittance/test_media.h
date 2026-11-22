#ifndef SILVER_LINING_TEST_MEDIA_H
#define SILVER_LINING_TEST_MEDIA_H

// Test-only fixture media for the transmittance experiments. NOT used by the
// renderer. The linear ramp is a deliberately simple heterogeneous medium whose
// optical depth along any straight ray has a closed form, so ratio tracking and
// delta tracking can be checked against an exact answer rather than another
// stochastic estimator.

#include "volume/medium.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CGL { namespace Volume {

// density(p) = a + (b - a) * (p.z - bounds.min.z) / extent.z inside the box,
// sigma_t = density * extinction_scale. Nonnegative when a, b >= 0.
class LinearRampMedium final : public Medium {
 public:
  // majorant_override < 0 means "use the true maximum" (the valid bound).
  // A smaller positive override is a deliberately INVALID majorant used to
  // confirm violations are reported.
  LinearRampMedium(const BBox& bounds, double a, double b, double extinction_scale,
                   double albedo_value = 0.5, double majorant_override = -1.0)
      : box(bounds), a(a), b(b), scale(extinction_scale), albedo_value(albedo_value) {
    if (!(a >= 0.0) || !(b >= 0.0) || !(scale >= 0.0))
      throw std::invalid_argument("LinearRampMedium requires a, b, scale >= 0");
    majorant = majorant_override >= 0.0 ? majorant_override
                                        : scale * std::max(a, b);
  }

  const BBox& bounds() const override { return box; }
  double density(const Vector3D& p) const override {
    for (int axis = 0; axis < 3; ++axis)
      if (p[axis] < box.min[axis] || p[axis] > box.max[axis]) return 0.0;
    const double u = (p.z - box.min.z) / (box.max.z - box.min.z);
    return a + (b - a) * u;
  }
  double sigma_t(const Vector3D& p) const override { return density(p) * scale; }
  double albedo(const Vector3D&) const override { return albedo_value; }
  double majorant_sigma_t() const override { return majorant; }

  // Delta tracking under the team convention (candidates at the majorant,
  // accepted with probability sigma_t / majorant; total extinction).
  Collision sample_collision(const Ray& ray, Interval segment, RNG& rng) const override {
    Collision result;
    if (segment.empty() || !(majorant > 0.0)) return result;
    double t = segment.begin;
    while (true) {
      t += -std::log1p(-rng.uniform()) / majorant;
      if (t >= segment.end) return result;
      const Vector3D p = ray.at_time(t);
      const double sigma = sigma_t(p);
      if (!(sigma <= majorant * (1.0 + 1e-9)))
        throw std::runtime_error("Majorant violation in delta tracking");
      if (rng.uniform() * majorant < sigma) {
        result.occurred = true;
        result.t = t;
        result.position = p;
        return result;
      }
    }
  }

  // Not used by the experiments; the free function in transmittance.h is.
  double transmittance(const Ray&, Interval, RNG&) const override {
    throw std::logic_error("Use ratio_tracking_transmittance for LinearRampMedium");
  }

  // Closed-form optical depth along the straight ray over [segment.begin, end].
  // sigma_t(t) = scale * (c0 + k * d.z * t) is linear in the ray parameter t.
  double exact_optical_depth(const Ray& ray, Interval segment) const {
    if (segment.empty()) return 0.0;
    const double k = (b - a) / (box.max.z - box.min.z);
    const double c0 = a + k * (ray.o.z - box.min.z);
    const double t0 = segment.begin, t1 = segment.end;
    return scale * (c0 * (t1 - t0) + 0.5 * k * ray.d.z * (t1 * t1 - t0 * t0));
  }

 private:
  BBox box;
  double a, b, scale, albedo_value, majorant;
};

} } // namespace CGL::Volume
#endif
