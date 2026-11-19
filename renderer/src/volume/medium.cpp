#include "medium.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace CGL { namespace Volume {

bool Medium::clip(const Ray& ray, double endpoint, Interval& segment) const {
  segment = Interval{};
  double lo = std::max(0.0, ray.min_t);
  double hi = std::min(endpoint, ray.max_t);
  if (!(hi > lo)) return false;
  const BBox& b = bounds();
  // Explicit parallel handling avoids 0 * infinity when on a box face.
  for (int axis = 0; axis < 3; ++axis) {
    if (ray.d[axis] == 0.0) {
      if (ray.o[axis] < b.min[axis] || ray.o[axis] > b.max[axis]) return false;
      continue;
    }
    double a = (b.min[axis] - ray.o[axis]) / ray.d[axis];
    double c = (b.max[axis] - ray.o[axis]) / ray.d[axis];
    if (a > c) std::swap(a, c);
    lo = std::max(lo, a);
    hi = std::min(hi, c);
    if (!(hi > lo)) return false;
  }
  segment.begin = lo;
  segment.end = hi;
  return true;
}

HomogeneousMedium::HomogeneousMedium(const BBox& bounds, double density,
    double extinction_scale, double albedo)
    : box(bounds), constant_density(density), extinction(density * extinction_scale),
      scattering_albedo(albedo) {
  for (int axis = 0; axis < 3; ++axis)
    if (!std::isfinite(box.min[axis]) || !std::isfinite(box.max[axis]) ||
        !(box.max[axis] > box.min[axis]))
      throw std::invalid_argument("Volume bounds must be finite and strictly ordered");
  if (!std::isfinite(density) || density < 0 || !std::isfinite(extinction_scale) ||
      extinction_scale < 0 || !std::isfinite(extinction) ||
      !std::isfinite(albedo) || albedo < 0 || albedo > 1)
    throw std::invalid_argument("Volume requires density/scale >= 0 and albedo in [0,1]");
}

double HomogeneousMedium::density(const Vector3D& p) const {
  for (int axis = 0; axis < 3; ++axis)
    if (p[axis] < box.min[axis] || p[axis] > box.max[axis]) return 0.0;
  return constant_density;
}
double HomogeneousMedium::sigma_t(const Vector3D& p) const {
  return density(p) == 0.0 ? 0.0 : extinction;
}

Collision HomogeneousMedium::sample_collision(const Ray& ray, Interval segment, RNG& rng) const {
  Collision result;
  if (segment.empty() || extinction == 0.0) return result;
  const double distance = -std::log1p(-rng.uniform()) / extinction;
  if (distance >= segment.end - segment.begin) return result;
  result.occurred = true;
  result.t = segment.begin + distance;
  result.position = ray.at_time(result.t);
  return result;
}

double HomogeneousMedium::transmittance(const Ray&, Interval segment, RNG&) const {
  return segment.empty() || extinction == 0.0 ? 1.0 :
      std::exp(-extinction * (segment.end - segment.begin));
}

Settings load_settings(const std::string& filename) {
  std::ifstream file(filename);
  if (!file) throw std::runtime_error("Cannot open medium settings: " + filename);
  Settings result;
  Vector3D lo, hi;
  double density = 1.0, scale = 0.0, albedo = 0.0;
  std::set<std::string> seen;
  std::string text;
  size_t line_number = 0;
  while (std::getline(file, text)) {
    ++line_number;
    text = text.substr(0, text.find('#'));
    std::istringstream line(text);
    std::string key, extra;
    if (!(line >> key)) continue;
    const std::string error = filename + ":" + std::to_string(line_number) + ": ";
    if (!seen.insert(key).second) throw std::runtime_error(error + "Duplicate key " + key);
    bool valid = false;
    if (key == "bounds_min") valid = bool(line >> lo.x >> lo.y >> lo.z);
    else if (key == "bounds_max") valid = bool(line >> hi.x >> hi.y >> hi.z);
    else if (key == "density") valid = bool(line >> density);
    else if (key == "extinction_scale") valid = bool(line >> scale);
    else if (key == "albedo") valid = bool(line >> albedo);
    else if (key == "seed") {
      std::string seed;
      if (line >> seed) {
        if (seed.find_first_not_of("0123456789") != std::string::npos)
          throw std::runtime_error(error + "seed must be a nonnegative integer");
        result.seed = std::stoull(seed);
        valid = true;
      }
    } else throw std::runtime_error(error + "Unknown key " + key);
    if (!valid || (line >> extra)) throw std::runtime_error(error + "Invalid value for " + key);
  }
  for (const auto& required : {"bounds_min", "bounds_max", "extinction_scale", "albedo"})
    if (!seen.count(required)) throw std::runtime_error(filename + ": Missing key " + required);
  result.medium = std::make_shared<HomogeneousMedium>(BBox(lo, hi), density, scale, albedo);
  return result;
}
} }
