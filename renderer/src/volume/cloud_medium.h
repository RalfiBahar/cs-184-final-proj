#ifndef SILVER_LINING_CLOUD_MEDIUM_H
#define SILVER_LINING_CLOUD_MEDIUM_H

#include "medium.h"
#include "transmittance.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace CGL { namespace Volume {

class CloudMedium final : public Medium {
 public:
  // Samples include both endpoints on each axis, with x varying fastest.
  CloudMedium(const BBox& bounds, int resolution, std::vector<double> samples,
              double extinction_scale, double scattering_albedo)
      : box(bounds), size(resolution), grid(std::move(samples)),
        scale(extinction_scale), scattering(scattering_albedo) {
    if (size < 2 || size > 256 ||
        grid.size() != static_cast<size_t>(size) * size * size)
      throw std::invalid_argument("Cloud grid requires 2 to 256 samples per axis");
    for (int axis = 0; axis < 3; ++axis) {
      const double extent = box.max[axis] - box.min[axis];
      if (!(extent > 0) || !std::isfinite(extent))
        throw std::invalid_argument("Cloud bounds require finite positive extents");
    }
    if (!std::isfinite(scale) || scale < 0 ||
        !std::isfinite(scattering) || scattering < 0 || scattering > 1)
      throw std::invalid_argument("Invalid cloud extinction scale or albedo");
    double maximum = 0.0;
    for (double sample : grid) {
      if (!std::isfinite(sample) || sample < 0)
        throw std::invalid_argument("Cloud density samples must be finite and nonnegative");
      maximum = std::max(maximum, sample);
    }
    // Trilinear interpolation stays within the range of the corner samples.
    majorant = maximum * scale;
    if (majorant > 0 && std::isfinite(majorant))
      majorant = std::nextafter(majorant, std::numeric_limits<double>::infinity());
    if (!std::isfinite(majorant))
      throw std::invalid_argument("Cloud extinction majorant must be finite");
  }

  static CloudMedium procedural(const BBox& box, int size, uint64_t seed,
                                double scale, double albedo) {
    if (size < 2 || size > 256)
      throw std::invalid_argument("Cloud grid requires 2 to 256 samples per axis");
    std::vector<double> samples;
    samples.reserve(static_cast<size_t>(size) * size * size);
    for (int z = 0; z < size; ++z)
      for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
          const Vector3D p(x / double(size - 1), y / double(size - 1),
                           z / double(size - 1));
          const Vector3D centered = (p - Vector3D(0.5, 0.5, 0.5)) * 2.0;
          const double envelope = std::max(0.0, 1.0 - dot(centered, centered));
          double noise = 0;
          double amplitude = 8.0 / 15.0;
          for (int frequency = 4; frequency <= 32; frequency *= 2) {
            noise += amplitude * value_noise(p * frequency, seed);
            amplitude *= 0.5;
          }
          samples.push_back(envelope * std::max(0.0, (noise - 0.35) / 0.65));
        }
    return CloudMedium(box, size, std::move(samples), scale, albedo);
  }

  const BBox& bounds() const override { return box; }
  double density(const Vector3D& p) const override {
    int base[3];
    double fraction[3];
    for (int axis = 0; axis < 3; ++axis) {
      if (!(p[axis] >= box.min[axis] && p[axis] <= box.max[axis])) return 0;
      const double cell = (p[axis] - box.min[axis]) /
                          (box.max[axis] - box.min[axis]) * (size - 1);
      base[axis] = std::min(static_cast<int>(cell), size - 2);
      fraction[axis] = cell - base[axis];
    }
    return interpolate(base, fraction, [this](int x, int y, int z) {
      return grid[(z * size + y) * size + x];
    });
  }
  double sigma_t(const Vector3D& p) const override { return scale * density(p); }
  double albedo(const Vector3D&) const override { return scattering; }
  double majorant_sigma_t() const override { return majorant; }

  Collision sample_collision(const Ray& ray, Interval segment, RNG& rng) const override {
    if (segment.empty() || majorant == 0) return {};
    double t = segment.begin;
    while (t < segment.end) {
      const double distance = -std::log1p(-rng.uniform()) / majorant;
      const double next = t + distance;
      t = next > t ? next : std::nextafter(t, segment.end);
      if (!(t < segment.end)) break;
      const Vector3D p = ray.at_time(t);
      const double sigma = sigma_t(p);
      if (!std::isfinite(sigma) || sigma < 0 || sigma > majorant)
        throw std::runtime_error("Cloud extinction exceeds its majorant");
      if (rng.uniform() < sigma / majorant) return {true, t, p};
    }
    return {};
  }

  // Part D attenuation-only estimator; callers handle opaque visibility.
  double transmittance(const Ray& ray, Interval segment, RNG& rng) const override {
    return ratio_tracking_transmittance(*this, ray, segment, rng);
  }

 private:
  template <typename Sample>
  static double interpolate(const int* base, const double* f, Sample sample) {
    auto lerp = [](double a, double b, double t) { return a + (b - a) * t; };
    double planes[2];
    for (int z = 0; z < 2; ++z) {
      double rows[2];
      for (int y = 0; y < 2; ++y)
        rows[y] = lerp(sample(base[0], base[1] + y, base[2] + z),
                       sample(base[0] + 1, base[1] + y, base[2] + z), f[0]);
      planes[z] = lerp(rows[0], rows[1], f[1]);
    }
    return lerp(planes[0], planes[1], f[2]);
  }

  static double value_noise(const Vector3D& p, uint64_t seed) {
    int base[3];
    double fade[3];
    for (int axis = 0; axis < 3; ++axis) {
      base[axis] = static_cast<int>(p[axis]);
      const double t = p[axis] - base[axis];
      fade[axis] = t * t * (3.0 - 2.0 * t);
    }
    return interpolate(base, fade, [seed](int x, int y, int z) {
      uint64_t hash = (seed ^ (x + 37ULL * y + 1369ULL * z)) + 0x9e3779b97f4a7c15ULL;
      hash = (hash ^ (hash >> 30)) * 0xbf58476d1ce4e5b9ULL;
      hash = (hash ^ (hash >> 27)) * 0x94d049bb133111ebULL;
      return double(hash >> 11) / 9007199254740992.0;
    });
  }

  BBox box;
  int size;
  std::vector<double> grid;
  double scale;
  double scattering;
  double majorant;
};

} } // namespace CGL::Volume
#endif
