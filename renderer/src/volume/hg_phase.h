#ifndef SILVER_LINING_HG_PHASE_H
#define SILVER_LINING_HG_PHASE_H
#include "phase.h"
#include <stdexcept>

namespace CGL { namespace Volume {
// Uses the committed traveling-direction convention, not PBRT's both-away one.
class HenyeyGreensteinPhase final : public PhaseFunction {
 public:
  explicit HenyeyGreensteinPhase(double g) : g_(g) {
    if (!std::isfinite(g) || std::abs(g) >= 1.0)
      throw std::invalid_argument("HG requires finite -1 < g < 1");
  }
  double asymmetry() const { return g_; }
  double evaluate(const Vector3D& incoming, const Vector3D& outgoing) const override {
    const double mu = std::max(-1.0, std::min(1.0,
        dot(unit_direction(incoming), unit_direction(outgoing))));
    // Avoid cancellation at the narrow forward/backward peak.
    const double a = std::abs(g_);
    const double denominator = (1-a)*(1-a) + 2*a*(1-(g_ < 0 ? -mu : mu));
    return ((1-a)*(1+a)) / (4*PI*denominator*std::sqrt(denominator));
  }
  PhaseSample sample(const Vector3D& incoming, RNG& rng) const override {
    const Vector3D w = unit_direction(incoming);
    const double t = 2*rng.uniform()-1;
    double mu;
    if (g_ == 0.0) {
      mu = t; // Exact isotropic fallback.
    } else if (std::abs(g_) < 1e-3) {
      // Algebraically equivalent inverse CDF, without division by tiny g.
      const double d = 1+g_*t;
      mu = (2*t + g_*(t*t+3) + 2*g_*g_*t + g_*g_*g_*(t*t-1))/(2*d*d);
    } else {
      const double q = ((1-g_)*(1+g_))/(1+g_*t);
      mu = (1+g_*g_-q*q)/(2*g_);
    }
    mu = std::max(-1.0, std::min(1.0, mu)); // Roundoff only.
    const double phi = 2*PI*rng.uniform();
    const Vector3D axis = std::abs(w.z) < 0.9 ? Vector3D(0,0,1) : Vector3D(1,0,0);
    const Vector3D u = cross(axis,w).unit(), v = cross(w,u);
    const double radial = std::sqrt(std::max(0.0,(1-mu)*(1+mu)));
    PhaseSample s;
    s.direction = (mu*w + radial*(std::cos(phi)*u + std::sin(phi)*v)).unit();
    s.value = s.pdf = evaluate(w,s.direction);
    return s;
  }
 private:
  static Vector3D unit_direction(const Vector3D& d) {
    const double n = std::hypot(std::hypot(d.x,d.y),d.z);
    if (!(n > 0) || !std::isfinite(n))
      throw std::invalid_argument("HG requires finite nonzero directions");
    return d/n;
  }
  double g_;
};
} }
#endif
