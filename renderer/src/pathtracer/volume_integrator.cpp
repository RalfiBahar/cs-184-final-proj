#include "pathtracer.h"
#include "scene/light.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CGL {
using namespace SceneObjects;

double PathTracer::medium_transmittance(const Ray& shadow, Volume::RNG& rng) const {
  if (!medium) return 1.0;
  Volume::Interval interval;
  return medium->clip(shadow, shadow.max_t, interval) ?
      medium->transmittance(shadow, interval, rng) : 1.0;
}

Vector3D PathTracer::volume_direct_lighting(const Vector3D& position,
    const Vector3D& incoming, Volume::RNG& rng) const {
  Vector3D result(0.0);
  // Every light is sampled, so there is no additional light-selection PDF.
  for (auto light : scene->lights) {
    const size_t count = light->is_delta_light() ? 1 : ns_area_light;
    if (!count) continue;
    Vector3D sum(0.0);
    for (size_t sample = 0; sample < count; ++sample) {
      Vector3D wi;
      double distance = 0.0, pdf = 0.0;
      const Vector3D Li = light->sample_L(position, &wi, &distance, &pdf);
      if (!(pdf > 0.0) || !std::isfinite(pdf) || !(distance > EPS_F)) continue;
      Ray shadow(position, wi.unit());
      shadow.min_t = EPS_F;
      shadow.max_t = distance - EPS_F;
      // Visibility is separate from attenuation. Bounds never block light.
      if (bvh->has_intersection(shadow)) continue;
      sum += Li * (phase->evaluate(incoming, shadow.d) *
                   medium_transmittance(shadow, rng) / pdf);
    }
    result += sum / double(count);
  }
  return result;
}

Vector3D PathTracer::surface_direct_lighting_volume(const Ray& ray,
    const Intersection& hit, Volume::RNG& rng) const {
  Matrix3x3 o2w;
  make_coord_space(o2w, hit.n);
  const Matrix3x3 w2o = o2w.T();
  const Vector3D position = ray.at_time(hit.t);
  const Vector3D wo = w2o * (-ray.d);
  Vector3D result(0.0);
  for (auto light : scene->lights) {
    const size_t count = light->is_delta_light() ? 1 : ns_area_light;
    if (!count) continue;
    Vector3D sum(0.0);
    for (size_t sample = 0; sample < count; ++sample) {
      Vector3D wi;
      double distance = 0.0, pdf = 0.0;
      const Vector3D Li = light->sample_L(position, &wi, &distance, &pdf);
      if (!(pdf > 0.0) || !std::isfinite(pdf) || !(distance > EPS_F)) continue;
      const Vector3D local = w2o * wi;
      if (local.z <= 0.0) continue;
      Ray shadow(position, wi.unit());
      shadow.min_t = EPS_F;
      shadow.max_t = distance - EPS_F;
      if (bvh->has_intersection(shadow)) continue;
      sum += Li * hit.bsdf->f(wo, local) *
             (local.z * medium_transmittance(shadow, rng) / pdf);
    }
    result += sum / double(count);
  }
  return result;
}

Vector3D PathTracer::est_radiance_volume(const Ray& input, Volume::RNG& rng) {
  // Accumulated radiance only: CLI rejects HW bounce-isolation / hemisphere
  // modes with a volume rather than silently changing their meaning.
  Ray ray = input;
  Vector3D L(0.0), throughput(1.0);
  bool allow_emission = true; // camera or previous delta BSDF
  size_t scattering_depth = 0;
  for (;;) {
    Intersection hit;
    // Primitive intersections shrink Ray::max_t even through const references.
    // Keep that mutation on a copy, then clip the medium to the closest hit.
    Ray surface_ray = ray;
    const bool surface_found = bvh->intersect(surface_ray, &hit);
    Volume::Interval segment;
    Volume::Collision collision;
    if (medium->clip(ray, surface_found ? hit.t : ray.max_t, segment))
      collision = medium->sample_collision(ray, segment, rng);

    if (collision.occurred) {
      if (scattering_depth >= max_ray_depth) break;
      // Total-extinction free flight already accounts for segment survival.
      throughput *= medium->albedo(collision.position);
      if (std::max(throughput.x, std::max(throughput.y, throughput.z)) <= 0.0) break;
      L += throughput * volume_direct_lighting(collision.position, ray.d, rng);
      ++scattering_depth;
      if (scattering_depth >= max_ray_depth) break;
      const Volume::PhaseSample next = phase->sample(ray.d, rng);
      if (!(next.pdf > 0.0) || !std::isfinite(next.pdf)) break;
      throughput *= next.value / next.pdf;
      ray = Ray(collision.position, next.direction.unit());
      ray.min_t = EPS_F;
      allow_emission = false; // NEE already accounts for directly hit emitters
    } else {
      // On escape throughput is unchanged (do not multiply by Tr again).
      if (!surface_found) {
        if (allow_emission && envLight) L += throughput * envLight->sample_dir(ray);
        break;
      }
      if (allow_emission) L += throughput * hit.bsdf->get_emission();
      if (scattering_depth >= max_ray_depth) break;
      L += throughput * surface_direct_lighting_volume(ray, hit, rng);
      ++scattering_depth;
      if (scattering_depth >= max_ray_depth) break;
      Matrix3x3 o2w;
      make_coord_space(o2w, hit.n);
      const Vector3D wo = o2w.T() * (-ray.d);
      Vector3D wi;
      double pdf = 0.0;
      const Vector3D f = hit.bsdf->sample_f(wo, &wi, &pdf);
      if (!(pdf > 0.0) || !std::isfinite(pdf)) break;
      const double cosine = hit.bsdf->is_delta() ? std::abs(wi.z) : wi.z;
      if (!(cosine > 0.0)) break;
      throughput = throughput * f * (cosine / pdf);
      allow_emission = hit.bsdf->is_delta();
      ray = Ray(ray.at_time(hit.t), (o2w * wi).unit());
      ray.min_t = EPS_F;
    }
    if (scattering_depth >= 3) {
      const double p = std::min(0.95, std::max(0.05,
          std::max(throughput.x, std::max(throughput.y, throughput.z))));
      if (rng.uniform() >= p) break;
      throughput /= p;
    }
  }
  return L;
}
} // namespace CGL
