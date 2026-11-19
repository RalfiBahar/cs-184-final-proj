#include "pathtracer/pathtracer.h"
#include "scene/object.h"
#include "scene/light.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <limits>

using namespace CGL;
using namespace CGL::Volume;
using namespace CGL::SceneObjects;
namespace {
int checks = 0;
void check(bool good, const char* message) {
  ++checks;
  if (!good) throw std::runtime_error(message);
}
void near(double a, double b, double tolerance, const char* message) {
  if (std::abs(a-b) > tolerance) std::cerr << message << ": " << a << " vs " << b << "\n";
  check(std::abs(a-b) <= tolerance, message);
}
template<class F> void rejects(F f, const char* message) {
  bool rejected = false;
  try { f(); } catch (const std::exception&) { rejected = true; }
  check(rejected, message);
}

void geometry_and_sampling() {
  const BBox box(Vector3D(0,0,0),Vector3D(1,1,1));
  HomogeneousMedium m(box,1.0,0.8,0.7);
  RNG rng(9);
  Interval segment;
  Ray r(Vector3D(.5,.5,-1),Vector3D(0,0,1));
  check(m.clip(r,INF_D,segment),"outside ray enters volume");
  near(segment.begin,1,1e-12,"entry distance");
  near(segment.end,2,1e-12,"exit distance");
  near(m.transmittance(r,segment,rng),std::exp(-.8),1e-12,"analytic slab Tr");
  check(!m.clip(r,.5,segment),"surface in front of volume clips it out");
  check(m.clip(r,1.4,segment),"surface inside volume clips far endpoint");
  near(segment.end,1.4,1e-12,"nearest-surface endpoint");
  check(m.clip(Ray(Vector3D(.5,.5,.5),Vector3D(1,0,0)),INF_D,segment),"camera inside");
  near(segment.begin,0,1e-12,"inside ray starts at zero");
  near(segment.end,.5,1e-12,"inside ray exit");
  check(m.clip(Ray(Vector3D(0,.5,-1),Vector3D(0,0,1)),INF_D,segment),"parallel face is finite");
  check(!m.clip(Ray(Vector3D(-.1,.5,-1),Vector3D(0,0,1)),INF_D,segment),"parallel outside misses");
  check(!m.clip(Ray(Vector3D(.5,.5,-1),Vector3D(0,0,-1)),INF_D,segment),"behind ray misses");
  Ray short_ray=r; short_ray.max_t=1.25;
  check(m.clip(short_ray,5,segment),"ray max_t respected");
  near(segment.end,1.25,1e-12,"short ray endpoint");
  near(m.sigma_t(Vector3D(2)),0,0,"outside is vacuum");
  near(m.density(Vector3D(.5)),1,0,"inside density");
  HomogeneousMedium vacuum(box,0,1,1);
  check(vacuum.clip(r,INF_D,segment),"vacuum box still has bounds");
  check(!vacuum.sample_collision(r,segment,rng).occurred,"vacuum never collides");
  near(vacuum.transmittance(r,segment,rng),1,0,"vacuum Tr");
  near(m.transmittance(r,Interval{},rng),1,0,"empty segment Tr");
  rejects([&](){HomogeneousMedium bad(box,-1,1,1);},"negative density rejected");
  rejects([&](){HomogeneousMedium bad(box,1,1,1.01);},"invalid albedo rejected");
  rejects([&](){HomogeneousMedium bad(BBox(Vector3D(1),Vector3D(0)),1,1,1);},"invalid bounds rejected");
  m.clip(r,INF_D,segment);
  const int trials=150000;
  int escapes=0;
  for(int i=0;i<trials;++i) {
    const auto c=m.sample_collision(r,segment,rng);
    if (!c.occurred) ++escapes;
    else check(c.t>=segment.begin && c.t<segment.end,"collision in clipped interval");
  }
  const double expected=std::exp(-.8), measured=double(escapes)/trials;
  const double standard_error=std::sqrt(expected*(1-expected)/trials);
  near(measured,expected,5*standard_error,"free flight escape probability (5 SE)");
  std::cout << "escape_probability," << measured << ",analytic," << expected << ",SE," << standard_error << "\n";
  RNG a(184),b(184);
  for(int i=0;i<50;++i)near(a.uniform(),b.uniform(),0,"seed reproducibility");
}

// Small scenes test the actual integrator, not a duplicated reference algorithm.
struct Fixture {
  EmissionBSDF emitter{Vector3D(1)};
  DiffuseBSDF diffuse{Vector3D(1)};
  SphereObject object;
  std::vector<Primitive*> primitives;
  BVHAccel accel;
  Scene scene;
  PathTracer tracer;
  Fixture(Vector3D center, double radius, bool emissive,
          const std::vector<SceneLight*>& lights={})
      : object(center,radius,emissive ? static_cast<BSDF*>(&emitter) : static_cast<BSDF*>(&diffuse)),
        primitives(object.get_primitives()),accel(primitives),scene({},lights) {
    tracer.bvh=&accel; tracer.scene=&scene; tracer.envLight=nullptr;
    tracer.max_ray_depth=1; tracer.isAccumBounces=true; tracer.ns_area_light=1;
    tracer.direct_hemisphere_sample=false;
  }
  ~Fixture(){ for(auto primitive:primitives)delete primitive; }
};

double mean_ray(PathTracer& pt, const Ray& ray, int trials, double& standard_error) {
  RNG rng(12345);
  double sum=0,square=0;
  for(int i=0;i<trials;++i) {
    const Vector3D L=pt.est_radiance_volume(ray,rng);
    check(std::isfinite(L.x) && std::isfinite(L.y) && std::isfinite(L.z) &&
          L.x>=0 && L.y>=0 && L.z>=0,"finite nonnegative RGB radiance");
    sum+=L.x; square+=L.x*L.x;
  }
  const double mean=sum/trials;
  standard_error=std::sqrt(std::max(0.0,(square-trials*mean*mean)/(trials-1))/trials);
  return mean;
}

void integrator_tests() {
  const BBox slab(Vector3D(-1,-1,0),Vector3D(1,1,1));
  const Ray camera(Vector3D(0,0,-1),Vector3D(0,0,1));
  double se;
  // Emissive sphere's front point is inside the volume at z=.4.
  Fixture inside(Vector3D(0,0,.5),.1,true);
  inside.tracer.max_ray_depth=0;
  inside.tracer.medium=std::make_shared<HomogeneousMedium>(slab,1,.8,0);
  const double absorbed=mean_ray(inside.tracer,camera,100000,se);
  near(absorbed,std::exp(-.8*.4),5*se+.0001,"surface inside: sampled Tr applied once");
  std::cout << "inside_surface," << absorbed << ",analytic," << std::exp(-.32) << ",SE," << se << "\n";
  Fixture front(Vector3D(0,0,-.5),.1,true);
  front.tracer.medium=inside.tracer.medium;
  near(mean_ray(front.tracer,camera,10,se),1,0,"surface before fog never skipped");
  // Empty camera path, dummy primitive far off-axis; one directional light.
  DirectionalLight directional(Vector3D(1),Vector3D(0,0,-1));
  Fixture scattering(Vector3D(100,100,100),.1,false,{&directional});
  scattering.tracer.medium=std::make_shared<HomogeneousMedium>(slab,1,.8,.7);
  const double scattered=mean_ray(scattering.tracer,camera,180000,se);
  const double reference=.7*.8*std::exp(-.8)/(4*PI);
  near(scattered,reference,5*se+.00001,"single scattering analytic directional slab");
  std::cout << "single_scattering," << scattered << ",analytic," << reference << ",SE," << se << "\n";
  // Surface direct-light shadow crosses slab: camera Tr and shadow Tr both count.
  PointLight point(Vector3D(1),Vector3D(0,0,-2));
  Fixture surface(Vector3D(0,0,2),.2,false,{&point});
  surface.tracer.medium=std::make_shared<HomogeneousMedium>(slab,1,.8,0);
  const double shaded=mean_ray(surface.tracer,camera,120000,se);
  const double shading_ref=std::exp(-1.6)/PI;
  near(shaded,shading_ref,5*se+.00001,"surface NEE includes medium shadow Tr");
  std::cout << "surface_shadow," << shaded << ",analytic," << shading_ref << ",SE," << se << "\n";
  // Put an opaque sphere between the same scattering path and a side light.
  DirectionalLight side(Vector3D(1),Vector3D(-1,0,0));
  Fixture blocked(Vector3D(2,0,.5),1.2,false,{&side});
  blocked.tracer.medium=scattering.tracer.medium;
  near(mean_ray(blocked.tracer,camera,10000,se),0,0,"opaque blocker hides volume NEE");
  // Empty grid equivalent (zero coefficient) must agree on direct emission.
  inside.tracer.medium=std::make_shared<HomogeneousMedium>(slab,0,1,1);
  near(mean_ray(inside.tracer,camera,10,se),1,0,"zero density emission regression");
}

void config_and_phase() {
  std::ofstream f("test_medium.vol");
  f << "bounds_min -1 -1 0\nbounds_max 1 1 1\ndensity 2\nextinction_scale .4\nalbedo .7\nseed 184\n";
  f.close();
  const Settings s=load_settings("test_medium.vol");
  near(s.medium->sigma_t(Vector3D(0,0,.5)),.8,1e-12,"sidecar coefficients");
  check(s.seed==184,"sidecar seed");
  {std::ofstream bad("test_bad.vol");bad<<"unknown 7\n";}
  rejects([](){load_settings("test_bad.vol");},"unknown config key rejected");
  {std::ofstream bad("test_bad.vol");bad<<"bounds_min 0 0 0\n";}
  rejects([](){load_settings("test_bad.vol");},"missing config keys rejected");
  IsotropicPhase phase; RNG rng(8);Vector3D mean(0);
  for(int i=0;i<30000;++i){auto p=phase.sample(Vector3D(0,0,1),rng);
    near(p.direction.norm(),1,1e-12,"unit phase sample");
    near(p.value/p.pdf,1,0,"phase sampling weight");mean+=p.direction;}
  mean/=30000;
  check(mean.norm()<.025,"isotropic phase has zero mean direction");
}

// A deterministic phase test double isolates emitter visibility after scattering.
class ForwardPhase : public PhaseFunction {
 public:
  double evaluate(const Vector3D&, const Vector3D&) const override { return 1; }
  PhaseSample sample(const Vector3D& incoming, RNG&) const override {
    PhaseSample result; result.direction=incoming; result.value=result.pdf=1; return result;
  }
};

class ControlledBounce : public BSDF {
 public:
  explicit ControlledBounce(bool delta) : delta(delta) {}
  Vector3D f(const Vector3D, const Vector3D) override { return Vector3D(); }
  Vector3D sample_f(const Vector3D wo, Vector3D* wi, double* pdf) override {
    *wi=wo; *pdf=1; return Vector3D(1/std::abs(wo.z));
  }
  Vector3D get_emission() const override { return Vector3D(); }
  bool is_delta() const override { return delta; }
 private:
  bool delta;
};

void surface_emitter_policy() {
  for(bool delta : {false,true}) {
    ControlledBounce bounce(delta);
    EmissionBSDF emission(Vector3D(1));
    SphereObject receiver(Vector3D(0,0,2),.2,&bounce);
    SphereObject emitter(Vector3D(0,0,-2),.2,&emission);
    auto primitives=receiver.get_primitives();
    const auto other=emitter.get_primitives();
    primitives.insert(primitives.end(),other.begin(),other.end());
    {
      BVHAccel accel(primitives);
      Scene scene({},{});
      PathTracer tracer;
      tracer.bvh=&accel; tracer.scene=&scene; tracer.envLight=nullptr;
      tracer.max_ray_depth=2; tracer.ns_area_light=1;
      tracer.medium=std::make_shared<HomogeneousMedium>(
          BBox(Vector3D(-3),Vector3D(3)),0,1,1);
      RNG rng(184);
      near(tracer.est_radiance_volume(Ray(Vector3D(0),Vector3D(0,0,1)),rng).x,
           delta ? 1 : 0,0,delta ? "delta surface retains emitter visibility" :
                                  "nondelta surface suppresses duplicate emitter hit");
    }
    for(auto primitive:primitives) delete primitive;
  }
}

void acceptance_regressions() {
  const BBox slab(Vector3D(-1,-1,0),Vector3D(1,1,1));
  double se;
  Fixture emitter(Vector3D(0,0,2),.2,true);
  emitter.tracer.medium=std::make_shared<HomogeneousMedium>(slab,1,.8,0);
  const Ray inside_camera(Vector3D(0,0,.5),Vector3D(0,0,1));
  const double inside_result=mean_ray(emitter.tracer,inside_camera,100000,se);
  near(inside_result,std::exp(-.4),
       5*se+.0001,"camera inside: analytic absorption to exit");
  // After a real phase event, an emitter hit must not add the NEE contribution
  // a second time. No lights are registered here to isolate that policy alone.
  emitter.tracer.medium=std::make_shared<HomogeneousMedium>(slab,1,.8,1);
  emitter.tracer.phase=std::make_shared<ForwardPhase>();
  emitter.tracer.max_ray_depth=2;
  const Ray outside_camera(Vector3D(0,0,-1),Vector3D(0,0,1));
  const double emitter_result=mean_ray(emitter.tracer,outside_camera,100000,se);
  near(emitter_result,std::exp(-.8),
       5*se+.0001,"phase emitter hit suppressed after scattering");
  emitter.tracer.medium=std::make_shared<HomogeneousMedium>(slab,0,1,1);
  near(mean_ray(emitter.tracer,outside_camera,10,se),1,0,"vacuum primary emission at depth two");
  emitter.tracer.medium.reset();
  Ray legacy=outside_camera; legacy.depth=2;
  near(emitter.tracer.est_radiance_global_illumination(legacy).x,1,0,
       "legacy integrator primary emission");

  DirectionalLight light(Vector3D(1),Vector3D(0,0,-1));
  Fixture scattering(Vector3D(100),.1,false,{&light});
  scattering.tracer.medium=std::make_shared<HomogeneousMedium>(slab,1,.8,.7);
  scattering.tracer.max_ray_depth=0;
  near(mean_ray(scattering.tracer,outside_camera,1000,se),0,0,"depth zero excludes scattering");
  scattering.tracer.max_ray_depth=1;
  const double first=mean_ray(scattering.tracer,outside_camera,100000,se);
  const double first_se=se;
  scattering.tracer.max_ray_depth=4;
  const double multiple=mean_ray(scattering.tracer,outside_camera,100000,se);
  check(multiple+5*(se+first_se)>=first,"higher depth retains single-scattering energy");

  // An independent solid-angle reference exercises the actual area-light PDF,
  // sample averaging, and the absence of a surface cosine at a volume vertex.
  AreaLight area(Vector3D(1),Vector3D(0,0,2),Vector3D(0,0,-1),
                 Vector3D(2,0,0),Vector3D(0,2,0));
  Fixture area_scene(Vector3D(100),.1,false,{&area});
  area_scene.tracer.medium=std::make_shared<HomogeneousMedium>(slab,0,1,1);
  area_scene.tracer.ns_area_light=100000;
  RNG area_rng(7);
  const double solid_angle=4*std::atan(1.0/(2*std::sqrt(6.0)));
  near(area_scene.tracer.volume_direct_lighting(Vector3D(0),Vector3D(0,0,1),area_rng).x,
       solid_angle/(4*PI),.0005,"area-light NEE agrees with rectangle solid angle");

  const double inf=std::numeric_limits<double>::infinity();
  const double nan=std::numeric_limits<double>::quiet_NaN();
  rejects([&](){HomogeneousMedium bad(slab,nan,1,1);},"NaN density rejected");
  rejects([&](){HomogeneousMedium bad(slab,1,inf,1);},"infinite scale rejected");
  rejects([&](){HomogeneousMedium bad(slab,1,1,nan);},"NaN albedo rejected");
  for(const auto& suffix : {"density -1", "albedo 2", "seed -1", "density 1 extra",
                            "density 1\ndensity 2", "bounds_max 1 1 1"}) {
    std::ofstream bad("test_bad.vol");
    bad<<"bounds_min -1 -1 0\nbounds_max 1 1 1\nextinction_scale .8\nalbedo .7\n"<<suffix<<"\n";
    bad.close();
    rejects([](){load_settings("test_bad.vol");},"invalid sidecar rejected");
  }
}
}
int main() {
  try {geometry_and_sampling();config_and_phase();integrator_tests();acceptance_regressions();surface_emitter_policy();}
  catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
  std::cout<<"PASS: "<<checks<<" checks (including Monte Carlo trials)\n";
  return 0;
}
