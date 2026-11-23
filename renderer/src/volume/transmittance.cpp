#include "volume/transmittance.h"
#include <cmath>
#include <stdexcept>

namespace CGL { namespace Volume {

double analytic_homogeneous_transmittance(double sigma_t, double distance) {
  if (!(sigma_t > 0.0) || !(distance > 0.0)) return 1.0;
  return std::exp(-sigma_t * distance);
}

double ratio_tracking_transmittance(const Medium& medium, const Ray& ray,
                                    Interval segment, RNG& rng,
                                    TransmittanceStats* stats) {
  const double majorant = medium.majorant_sigma_t();
  if (segment.empty() || !(majorant > 0.0)) return 1.0;
  double estimate = 1.0;
  double t = segment.begin;
  while (true) {
    t += -std::log1p(-rng.uniform()) / majorant;
    if (t >= segment.end) break;  // no more candidates before the endpoint
    if (stats) ++stats->candidates;
    const double sigma = medium.sigma_t(ray.at_time(t));
    // Small relative slack only absorbs floating-point rounding in the bound.
    if (!(sigma <= majorant * (1.0 + 1e-9)) || sigma < 0.0)
      throw std::runtime_error("Majorant violation in ratio tracking: sigma_t outside [0, majorant]");
    estimate *= 1.0 - sigma / majorant;
    if (estimate == 0.0) break;  // contribution is exactly zero from here on
  }
  return estimate;
}

double optical_depth_quadrature(const Medium& medium, const Ray& ray,
                                Interval segment, int intervals) {
  if (segment.empty()) return 0.0;
  if (intervals < 2) intervals = 2;
  if (intervals % 2) ++intervals;
  const double h = (segment.end - segment.begin) / intervals;
  double sum = medium.sigma_t(ray.at_time(segment.begin)) +
               medium.sigma_t(ray.at_time(segment.end));
  for (int i = 1; i < intervals; ++i)
    sum += (i % 2 ? 4.0 : 2.0) * medium.sigma_t(ray.at_time(segment.begin + i * h));
  return sum * h / 3.0;
}

} } // namespace CGL::Volume
