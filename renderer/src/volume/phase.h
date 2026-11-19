#ifndef SILVER_LINING_PHASE_H
#define SILVER_LINING_PHASE_H
#include "medium.h"
#include <cmath>

namespace CGL { namespace Volume {
struct PhaseSample {
  Vector3D direction;
  double value = 0.0;
  double pdf = 0.0;
};

// Traveling direction convention: incoming is the ray's travel direction,
// outgoing points toward the next vertex. Positive HG g should favor their
// dot product near +1. B can implement HG as a derived class; default is g=0.
class PhaseFunction {
 public:
  virtual ~PhaseFunction() = default;
  virtual double evaluate(const Vector3D& incoming, const Vector3D& outgoing) const = 0;
  virtual PhaseSample sample(const Vector3D& incoming, RNG& rng) const = 0;
};

class IsotropicPhase final : public PhaseFunction {
 public:
  double evaluate(const Vector3D&, const Vector3D&) const override { return 1.0 / (4.0 * PI); }
  PhaseSample sample(const Vector3D&, RNG& rng) const override {
    const double z = 1.0 - 2.0 * rng.uniform();
    const double phi = 2.0 * PI * rng.uniform();
    const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    PhaseSample result;
    result.direction = Vector3D(r * std::cos(phi), r * std::sin(phi), z);
    result.value = result.pdf = 1.0 / (4.0 * PI);
    return result;
  }
};
} }
#endif
