// Exercises the production A/B/C/D interfaces and the actual radiance loop.
#define main part_a_unused_main
#include "volume_tests.cpp"
#undef main
#include "volume/cloud_medium.h"
#include "volume/hg_phase.h"
#include "volume/transmittance.h"
#include <cstdio>

namespace {
const BBox unit_box(Vector3D(0), Vector3D(1));

class InvalidMajorant : public Medium {
 public:
  double sigma = 1.0 + 5e-10, bound = 1.0;
  const BBox& bounds() const override { return unit_box; }
  double density(const Vector3D&) const override { return sigma; }
  double sigma_t(const Vector3D&) const override { return sigma; }
  double albedo(const Vector3D&) const override { return 1; }
  double majorant_sigma_t() const override { return bound; }
  Collision sample_collision(const Ray&, Interval, RNG&) const override { return {}; }
  double transmittance(const Ray& ray, Interval segment, RNG& rng) const override {
    return ratio_tracking_transmittance(*this, ray, segment, rng);
  }
};

void configuration() {
  const char* path = "test_combined.vol";
  const std::string base = "bounds_min 0 0 0\nbounds_max 1 1 1\n"
                           "density 1\nextinction_scale 5\nalbedo .7\nseed 184\n";
  auto write = [&](const std::string& extra) { std::ofstream file(path); file << base << extra; };
  write("cloud_resolution 48\nphase_g .5\n");
  const auto settings = load_settings(path);
  check(dynamic_cast<const CloudMedium*>(settings.medium.get()) != nullptr, "configured actual cloud type");
  check(dynamic_cast<const HenyeyGreensteinPhase*>(settings.phase.get()) != nullptr, "configured HG type");
  PathTracer tracer;
  tracer.set_volume(settings);
  check(tracer.medium == settings.medium && tracer.phase == settings.phase && tracer.volume_seed == 184,
        "application setup connects configured medium, phase, seed");
  tracer.clear();
  check(tracer.medium == settings.medium && tracer.phase == settings.phase, "rerender preserves all modules");
  write("");
  const auto legacy = load_settings(path);
  check(dynamic_cast<const HomogeneousMedium*>(legacy.medium.get()) != nullptr, "old sidecar remains homogeneous");
  check(dynamic_cast<const IsotropicPhase*>(legacy.phase.get()) != nullptr, "omitted phase_g remains isotropic");
  for (const std::string extra : {"phase_g 1\n", "phase_g -1\n", "phase_g nan\n",
                                  "phase_g .5 extra\n", "phase_g .5\nphase_g .6\n"}) {
    write(extra); rejects([&]() { load_settings(path); }, "invalid phase sidecar rejected");
  }
  for (double g : {-0.5, 0.0, 0.5, 0.8}) {
    write("cloud_resolution 3\nphase_g " + std::to_string(g) + "\n");
    near(load_settings(path).phase_g, g, 1e-12, "phase setting preserved");
  }
  std::remove(path);
}

void cloud_statistics() {
  // Linear z ramp: trilinear interpolation has exactly known integral tau=1.
  CloudMedium ramp(unit_box, 2, {.2,.2,.2,.2,1.8,1.8,1.8,1.8}, 1, .7);
  CloudMedium constant(unit_box, 2, std::vector<double>(8, 1), 1, .7);
  CloudMedium empty(unit_box, 2, std::vector<double>(8, 0), 1, .7);
  near(ramp.density(Vector3D(.5)), 1, 1e-12, "trilinear ramp midpoint");
  near(ramp.density(Vector3D(0)), .2, 1e-12, "lower grid endpoint");
  near(ramp.density(Vector3D(1)), 1.8, 1e-12, "upper grid endpoint");
  near(ramp.density(Vector3D(-.01)), 0, 0, "outside cloud is vacuum");
  const Ray ray(Vector3D(.5,.5,-1), Vector3D(0,0,1));
  for (const Medium* medium : {static_cast<const Medium*>(&constant), static_cast<const Medium*>(&ramp)}) {
    for (unsigned seed : {184u, 2026u}) {
      RNG collisions(seed), shadows(seed+17), explicit_ratio(seed+17);
      Interval segment;
      check(medium->clip(ray, INF_D, segment), "cloud ray enters");
      near(optical_depth_quadrature(*medium, ray, segment), 1, 1e-10, "independent optical depth");
      const int n = 100000; int escapes = 0; double sum = 0, square = 0; bool fractional = false;
      for (int i=0; i<n; ++i) {
        const auto collision = medium->sample_collision(ray, segment, collisions);
        if (!collision.occurred) ++escapes;
        else check(collision.t >= segment.begin && collision.t < segment.end, "delta collision in segment");
        const double v = medium->transmittance(ray, segment, shadows);
        check(std::isfinite(v) && v>=0 && v<=1, "ratio estimate in range");
        // This exact same-seed comparison fails if C silently uses escape tracking.
        near(v, ratio_tracking_transmittance(*medium, ray, segment, explicit_ratio), 0,
             "cloud virtual dispatch uses D ratio tracking");
        fractional = fractional || (v>1e-6 && v<1-1e-6);
        sum += v; square += v*v;
      }
      const double expected=std::exp(-1.0), mean=sum/n;
      const double se=std::sqrt(std::max(0.0,square/n-mean*mean)/(n-1));
      near(mean, expected, 5*se+1e-6, "cloud ratio tracking analytic reference");
      near(double(escapes)/n, expected, 5*std::sqrt(expected*(1-expected)/n), "cloud delta tracking reference");
      if (medium == &ramp) check(fractional, "heterogeneous ratio tracking returns fractional estimates");
      std::cout << "cloud_shadow," << seed << ",mean," << mean << ",analytic," << expected << ",SE," << se << "\n";
    }
  }
  Interval segment; ramp.clip(ray, 1.4, segment);
  near(segment.end, 1.4, 1e-12, "surface inside cloud clips endpoint");
  RNG rng(184);
  near(empty.transmittance(ray, segment, rng), 1, 0, "empty cloud shadow");
  check(!empty.sample_collision(ray, segment, rng).occurred, "empty cloud escapes");
  auto a=CloudMedium::procedural(unit_box,48,184,5,.93);
  auto b=CloudMedium::procedural(unit_box,48,184,5,.93);
  double maximum=0;
  std::ofstream slice("cloud_density_slice.pgm");
  check(bool(slice),"density slice output available");
  slice << "P2\n48 48\n255\n";
  for (int y=0; y<48; ++y) for (int x=0; x<48; ++x) {
    const Vector3D p(x/47.0,y/47.0,.5);
    const double v=a.density(p); maximum=std::max(maximum,v);
    slice << int(std::min(255.0,v*255)) << ((x==47) ? '\n' : ' ');
    near(v,b.density(p),0,"procedural density reproducibility");
    check(v>=0 && a.sigma_t(p)<=a.majorant_sigma_t(),"procedural majorant valid");
  }
  check(maximum>0,"procedural cloud has density");
  InvalidMajorant invalid;
  rejects([&]() { RNG r(1); invalid.transmittance(ray,{0,100},r); },
          "tiny majorant violation rejected instead of negative weight");
  invalid.bound=std::numeric_limits<double>::infinity();
  rejects([&]() { invalid.transmittance(ray,{0,1},rng); },"infinite majorant rejected");
  rejects([]() { analytic_homogeneous_transmittance(-1,1); },"negative extinction rejected");
  rejects([]() { analytic_homogeneous_transmittance(1,-1); },"negative distance rejected");
}

void all_parts_radiance() {
  // Varying-density cloud + HG + ratio shadows through A's actual integrator.
  // Forward directional illumination makes camera+shadow tau exactly one.
  const Ray camera(Vector3D(.5,.5,-1),Vector3D(0,0,1));
  DirectionalLight light(Vector3D(1),Vector3D(0,0,-1));
  Fixture f(Vector3D(100),.1,false,{&light});
  Settings settings;
  settings.medium=std::make_shared<CloudMedium>(unit_box,2,
      std::vector<double>{.2,.2,.2,.2,1.8,1.8,1.8,1.8},1,.7);
  for (double g : {0.0,0.5,0.8}) {
    settings.phase=std::make_shared<HenyeyGreensteinPhase>(g);
    f.tracer.set_volume(settings);
    double se; const double mean=mean_ray(f.tracer,camera,200000,se);
    const double forward=(1+g)/(4*PI*(1-g)*(1-g));
    const double reference=.7*std::exp(-1.0)*forward;
    near(mean,reference,5*se+1e-5,"A+B+C+D single-scattering analytic reference");
    std::cout << "combined_scattering,g," << g << ",mean," << mean << ",analytic," << reference << ",SE," << se << "\n";
  }
  // Surface direct-light shadow and camera extinction each cross the cloud.
  PointLight point(Vector3D(1),Vector3D(.5,.5,-2));
  Fixture surface(Vector3D(.5,.5,2),.2,false,{&point});
  surface.tracer.medium=std::make_shared<CloudMedium>(unit_box,2,
      std::vector<double>{.2,.2,.2,.2,1.8,1.8,1.8,1.8},1,0);
  double se; const double shaded=mean_ray(surface.tracer,camera,200000,se);
  near(shaded,std::exp(-2.0)/PI,5*se+1e-5,"surface NEE uses cloud ratio attenuation");
  DirectionalLight side(Vector3D(1),Vector3D(-1,0,0));
  Fixture blocked(Vector3D(2,.5,.5),1.2,false,{&side});
  blocked.tracer.medium=settings.medium;
  near(mean_ray(blocked.tracer,camera,10000,se),0,0,"opaque blocker hides cloud NEE");
}

void threaded_visibility() {
  Fixture f(Vector3D(0,0,2),.2,false);
  const Ray ray(Vector3D(0),Vector3D(0,0,1));
  std::vector<std::thread> workers;
  for(int t=0;t<4;++t)workers.emplace_back([&](){for(int i=0;i<1000;++i){
    Ray local_ray=ray; // Primitive visibility queries may shrink mutable max_t.
    f.accel.has_intersection(local_ray);
  }});
  for(auto& worker:workers)worker.join();
  check(f.accel.total_rays.load()==4000,"parallel BVH ray counts are initialized and atomic");
}
}

int main() {
  try { configuration(); cloud_statistics(); all_parts_radiance(); threaded_visibility(); }
  catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << "\n"; return 1; }
  std::cout << "PASS: " << checks << " combined A/B/C/D checks\n";
}
