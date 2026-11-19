#include "pathtracer.h"

#include "scene/light.h"
#include "scene/sphere.h"
#include "scene/triangle.h"
#include "util/random_util.h"
#include <algorithm>
#include <cmath>

using namespace CGL::SceneObjects;

namespace CGL {

PathTracer::PathTracer() {
  phase = std::make_shared<Volume::IsotropicPhase>();
  gridSampler = new UniformGridSampler2D();
  hemisphereSampler = new UniformHemisphereSampler3D();

  tm_gamma = 2.2f;
  tm_level = 1.0f;
  tm_key = 0.18;
  tm_wht = 5.0f;
}

PathTracer::~PathTracer() {
  delete gridSampler;
  delete hemisphereSampler;
}

void PathTracer::set_frame_size(size_t width, size_t height) {
  sampleBuffer.resize(width, height);
  sampleCountBuffer.resize(width * height);
}

void PathTracer::clear() {
  bvh = NULL;
  scene = NULL;
  camera = NULL;
  sampleBuffer.clear();
  sampleCountBuffer.clear();
  sampleBuffer.resize(0, 0);
  sampleCountBuffer.resize(0, 0);
}

void PathTracer::write_to_framebuffer(ImageBuffer &framebuffer, size_t x0,
                                      size_t y0, size_t x1, size_t y1) {
  sampleBuffer.toColor(framebuffer, x0, y0, x1, y1);
}

Vector3D
PathTracer::estimate_direct_lighting_hemisphere(const Ray &r,
                                                const Intersection &isect) {
    Matrix3x3 o2w;
    make_coord_space(o2w, isect.n);
    Matrix3x3 w2o = o2w.T();

    const Vector3D hit_p = r.o + r.d * isect.t;
    const Vector3D w_out = w2o * (-r.d);
    const int num_samples = scene->lights.size() * ns_area_light;
    Vector3D L_out(0.0);

    if (num_samples == 0) return L_out;

    const double pdf = 1.0 / (2.0 * PI);

    for (int sample = 0; sample < num_samples; ++sample) {
        const Vector3D w_in = hemisphereSampler->get_sample();
        Ray sample_ray(hit_p, o2w * w_in);
        sample_ray.min_t = EPS_F;

        Intersection light_hit;
        if (bvh->intersect(sample_ray, &light_hit)) {
            const Vector3D emission = light_hit.bsdf->get_emission();
            const Vector3D bsdf = isect.bsdf->f(w_out, w_in);
            L_out += emission * bsdf * (cos_theta(w_in) / pdf);
        }
    }

    return L_out / num_samples;
}

Vector3D
PathTracer::estimate_direct_lighting_importance(const Ray &r,
                                                const Intersection &isect) {
  // Estimate the lighting from this intersection coming directly from a light.
  // To implement importance sampling, sample only from lights, not uniformly in
  // a hemisphere.
    Matrix3x3 o2w;
    make_coord_space(o2w, isect.n);
    Matrix3x3 w2o = o2w.T();

    const Vector3D hit_p = r.o + r.d * isect.t;
    const Vector3D w_out = w2o * (-r.d);
    Vector3D L_out(0.0);

    for (auto light : scene->lights) {
        const int num_samples = light->is_delta_light() ? 1 : ns_area_light;
        if (num_samples <= 0) continue;

        Vector3D light_sum(0.0);
        for (int sample = 0; sample < num_samples; ++sample) {
            Vector3D w_in_world;
            double dist_to_light, pdf;
            const Vector3D incoming =
                light->sample_L(hit_p, &w_in_world, &dist_to_light, &pdf);
            if (pdf <= 0.0) continue;

            const Vector3D w_in = w2o * w_in_world;
            const double cosine = cos_theta(w_in);
            if (cosine <= 0.0) continue;

            Ray shadow_ray(hit_p, w_in_world);
            shadow_ray.min_t = EPS_F;
            shadow_ray.max_t = dist_to_light - EPS_F;
            if (shadow_ray.max_t <= shadow_ray.min_t) continue;
            if (bvh->has_intersection(shadow_ray)) continue;

            light_sum += incoming * isect.bsdf->f(w_out, w_in) * (cosine / pdf);
        }

        L_out += light_sum / num_samples;
    }

    return L_out;

}

Vector3D PathTracer::zero_bounce_radiance(const Ray &r,
                                          const Intersection &isect) {
  // Returns the light that results from no bounces of light
    return isect.bsdf->get_emission();
}

Vector3D PathTracer::one_bounce_radiance(const Ray &r,
                                         const Intersection &isect) {
  // Returns either the direct illumination by hemisphere or importance sampling
  // depending on `direct_hemisphere_sample
    if (direct_hemisphere_sample) {
        return estimate_direct_lighting_hemisphere(r, isect);
    }
    return estimate_direct_lighting_importance(r, isect);
}

Vector3D PathTracer::at_least_one_bounce_radiance(const Ray &r,
                                                  const Intersection &isect) {
    if (r.depth <= 0) return Vector3D(0.0);

    Vector3D L_out(0.0);
    if (isAccumBounces || r.depth == 1) {
        L_out = one_bounce_radiance(r, isect);
    }
    if (r.depth == 1) return L_out;

    const double continuation_prob = (r.depth == max_ray_depth) ? 1.0 : 0.65;
    if (continuation_prob < 1.0 && !coin_flip(continuation_prob)) {
        return L_out;
    }

    Matrix3x3 o2w;
    make_coord_space(o2w, isect.n);
    Matrix3x3 w2o = o2w.T();

    const Vector3D hit_p = r.at_time(isect.t);
    const Vector3D w_out = w2o * (-r.d);

    Vector3D w_in;
    double pdf;
    const Vector3D bsdf = isect.bsdf->sample_f(w_out, &w_in, &pdf);
    const double cosine = cos_theta(w_in);
    if (pdf <= 0.0 || cosine <= 0.0) return L_out;

    Ray bounce_ray(hit_p, o2w * w_in);
    bounce_ray.min_t = EPS_F;
    bounce_ray.depth = r.depth - 1;

    Intersection next_isect;
    if (bvh->intersect(bounce_ray, &next_isect)) {
        const Vector3D bounced =
            at_least_one_bounce_radiance(bounce_ray, next_isect);
        L_out += bsdf * bounced * (cosine / (pdf * continuation_prob));
    }

    return L_out;
}

Vector3D PathTracer::est_radiance_global_illumination(const Ray &r) {
  // You will extend this in assignment 3-2.
  // If no intersection occurs, we simply return black.
  // This changes if you implement hemispherical lighting for extra credit.

  // The following line of code returns a debug color depending
  // on whether ray intersection with triangles or spheres has
  // been implemented.
  //
    if (medium) {
        Volume::RNG rng(volume_seed);
        return est_radiance_volume(r, rng);
    }
    Intersection isect;
    if (!bvh->intersect(r, &isect)) {
        return envLight ? envLight->sample_dir(r) : Vector3D(0.0);
    }

    Vector3D L_out(0.0);
    if (isAccumBounces || r.depth == 0) {
        L_out = zero_bounce_radiance(r, isect);
    }
    if (r.depth > 0) {
        L_out += at_least_one_bounce_radiance(r, isect);
    }
    return L_out;
}

void PathTracer::raytrace_pixel(size_t x, size_t y) {
    Vector3D radiance(0.0);
    double sum_illum = 0.0;
    double sum_illum_squared = 0.0;
    size_t count = 0;

    while (count < ns_aa) {
        Vector2D offset = gridSampler->get_sample();

        constexpr bool use_jittered_sampling = true;
        if (use_jittered_sampling && ns_aa == 16) {
            constexpr size_t grid_size = 4;
            const size_t col = count % grid_size;
            const size_t row = count / grid_size;

            offset.x = (col + offset.x) / grid_size;
            offset.y = (row + offset.y) / grid_size;
        }

        const double u = (x + offset.x) / sampleBuffer.w;
        const double v = (y + offset.y) / sampleBuffer.h;

        Ray ray = camera->generate_ray(u, v);
        ray.depth = max_ray_depth;
        // Independent reproducible volume stream for each pixel/sample. Legacy
        // BSDF/light samplers remain worker-local (see random_util.h).
        Vector3D sample;
        if (medium) {
            const uint64_t key = volume_seed ^
                (uint64_t(x + y * sampleBuffer.w) * 0x9e3779b97f4a7c15ULL) ^
                (uint64_t(count + 1) * 0xbf58476d1ce4e5b9ULL);
            Volume::RNG volume_rng(key);
            sample = est_radiance_volume(ray, volume_rng);
        } else {
            sample = est_radiance_global_illumination(ray);
        }

        radiance += sample;
        const double illum = sample.illum();
        sum_illum += illum;
        sum_illum_squared += illum * illum;
        ++count;

        if (count > 1 && samplesPerBatch > 0 &&
            count % samplesPerBatch == 0) {
            const double n = static_cast<double>(count);
            const double mean = sum_illum / n;
            const double variance = std::max(
                0.0, (sum_illum_squared - sum_illum * sum_illum / n) / (n - 1.0));
            const double interval = 1.96 * std::sqrt(variance / n);

            if (interval <= maxTolerance * mean) break;
        }
    }

    if (count > 0) {
        sampleBuffer.update_pixel(radiance / static_cast<double>(count), x, y);
    }
    sampleCountBuffer[x + y * sampleBuffer.w] = count;
}

void PathTracer::autofocus(Vector2D loc) {
  Ray r = camera->generate_ray(loc.x / sampleBuffer.w, loc.y / sampleBuffer.h);
  Intersection isect;

  bvh->intersect(r, &isect);

  camera->focalDistance = isect.t;
}

} // namespace CGL
