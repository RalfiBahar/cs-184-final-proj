#ifndef SILVER_LINING_MEDIUM_H
#define SILVER_LINING_MEDIUM_H

#include "scene/bbox.h"
#include <cstdint>
#include <memory>
#include <random>
#include <string>

namespace CGL { namespace Volume {

class PhaseFunction;

// One RNG per camera path; no shared mutable engine in volume modules.
class RNG {
 public:
  explicit RNG(uint64_t seed = 184) : engine(seed) {}
  double uniform() { return std::generate_canonical<double, 53>(engine); }
 private:
  std::mt19937_64 engine;
};

struct Interval {
  double begin = 0.0;
  double end = 0.0;
  bool empty() const { return !(end > begin); }
};

struct Collision {
  bool occurred = false;
  double t = 0.0; // absolute parameter along the original ray, not from entry
  Vector3D position;
};

// One immutable, bounded, nonemissive medium; vacuum outside.
// Callers provide UNIT direction rays. Intervals are absolute world distances.
// Sampling uses TOTAL extinction: collision throughput *= albedo(position).
// Escape throughput *= 1. Do NOT multiply sampled segments by transmittance.
class Medium {
 public:
  virtual ~Medium() = default;
  virtual const BBox& bounds() const = 0;
  virtual double density(const Vector3D& p) const = 0;
  virtual double sigma_t(const Vector3D& p) const = 0;
  virtual double albedo(const Vector3D& p) const = 0;
  virtual double majorant_sigma_t() const = 0;
  virtual Collision sample_collision(const Ray& ray, Interval segment, RNG& rng) const = 0;
  // Attenuation ONLY, without geometry visibility. D overrides with ratio
  // tracking; this is deliberately virtual to avoid integrating density here.
  virtual double transmittance(const Ray& ray, Interval segment, RNG& rng) const = 0;
  bool clip(const Ray& ray, double endpoint, Interval& segment) const;
};

class HomogeneousMedium final : public Medium {
 public:
  HomogeneousMedium(const BBox& bounds, double density, double extinction_scale,
                    double scattering_albedo);
  const BBox& bounds() const override { return box; }
  double density(const Vector3D& p) const override;
  double sigma_t(const Vector3D& p) const override;
  double albedo(const Vector3D&) const override { return scattering_albedo; }
  double majorant_sigma_t() const override { return extinction; }
  Collision sample_collision(const Ray& ray, Interval segment, RNG& rng) const override;
  double transmittance(const Ray& ray, Interval segment, RNG& rng) const override;
 private:
  BBox box;
  double constant_density;
  double extinction;
  double scattering_albedo;
};

// Strict text sidecar used with -v <file>. Omit -v for unchanged HW3 mode.
struct Settings {
  std::shared_ptr<const Medium> medium;
  std::shared_ptr<const PhaseFunction> phase;
  double phase_g = 0.0;
  uint64_t seed = 184;
};
Settings load_settings(const std::string& filename);

} } // namespace CGL::Volume
#endif
