#ifndef SILVER_LINING_DIRECT_LIGHTING_H
#define SILVER_LINING_DIRECT_LIGHTING_H
#include "phase.h"
#include "scene/bvh.h"
#include <stdexcept>

namespace CGL { namespace Volume {
// Extracted from A's estimator. Caller multiplies by path throughput/albedo.
// Tr(shadow, rng) must clip to the medium and return attenuation only.
// Each light is sampled separately: do NOT divide by number of lights.
template<class Transmittance>
Vector3D direct_volume_lighting(const Vector3D& position,
    const Vector3D& incoming, const PhaseFunction& phase,
    const std::vector<SceneObjects::SceneLight*>& lights,
    const SceneObjects::BVHAccel& bvh, size_t area_samples,
    RNG& rng, Transmittance transmittance) {
  if (!area_samples) throw std::invalid_argument("Volume lighting needs positive sample count");
  Vector3D result(0.0);
  for (auto light : lights) {
    const size_t count = light->is_delta_light() ? 1 : area_samples;
    Vector3D sum(0.0);
    for (size_t i=0; i<count; ++i) {
      Vector3D wi;
      double distance=0, pdf=0;
      const Vector3D Li = light->sample_L(position,&wi,&distance,&pdf);
      if (!(pdf>0) || !std::isfinite(pdf) || !(distance>2*EPS_F)) continue;
      const double length = wi.norm();
      if (!(length>0) || !std::isfinite(length)) continue;
      Ray shadow(position,wi/length);
      shadow.min_t=EPS_F;
      shadow.max_t=distance-EPS_F;
      // Some intersection APIs mutate the ray; preserve attenuation endpoints.
      Ray visibility_ray=shadow;
      if (bvh.has_intersection(visibility_ray)) continue;
      const double tr=transmittance(shadow,rng);
      if (!std::isfinite(tr) || tr<0 || tr>1)
        throw std::runtime_error("Invalid scalar shadow transmittance");
      sum += Li*(phase.evaluate(incoming,shadow.d)*tr/pdf);
    }
    result += sum/double(count);
  }
  return result;
}
} }
#endif
