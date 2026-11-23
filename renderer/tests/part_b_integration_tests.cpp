// Run A's original tests, then exercise B through the actual volume integrator.
#define main part_a_regression_main
#include "volume_tests.cpp"
#undef main
#include "volume/hg_phase.h"
#include "util/lodepng.h"
#include <iomanip>

void hg_integrator_checks(const std::string& out) {
  std::ofstream report(out+"/integrated_hg.csv");
  check(bool(report),"integration output directory exists");
  report<<std::setprecision(17)<<"g,seed,n,estimate,reference,standard_error\n";
  const BBox slab(Vector3D(-1,-1,0),Vector3D(1,1,1));
  const Ray camera(Vector3D(0,0,-1),Vector3D(0,0,1));
  DirectionalLight light(Vector3D(1),Vector3D(0,0,-1));
  Fixture f(Vector3D(100),.1,false,{&light});
  f.accel.total_rays=f.accel.total_isects=0;
  f.tracer.medium=std::make_shared<HomogeneousMedium>(slab,1,.8,.7);
  for(double g:{-.5,0.,.5,.8}) {
    f.tracer.phase=std::make_shared<HenyeyGreensteinPhase>(g);
    // Independent forward-direction phase formula plus analytic slab integral.
    const double p=(1+g)/(4*PI*(1-g)*(1-g));
    const double reference=.7*.8*std::exp(-.8)*p;
    for(unsigned seed:{184u,2026u,9173u}) {
      RNG rng(seed);const int n=100000;double sum=0,sq=0;
      for(int i=0;i<n;++i){const auto L=f.tracer.est_radiance_volume(camera,rng);check(std::isfinite(L.x)&&L.x>=0,"finite integrated radiance");sum+=L.x;sq+=L.x*L.x;}
      const double mean=sum/n,se=std::sqrt(std::max(0.,sq/n-mean*mean)/(n-1));
      near(mean,reference,5*se+1e-5,"integrated HG slab agrees with analytic reference");
      report<<g<<','<<seed<<','<<n<<','<<mean<<','<<reference<<','<<se<<'\n';
    }
  }
}
void render_hg_comparison(const std::string& out) {
  // A small synthetic scene using real sphere geometry, medium, and integrator.
  // Fixed orthographic camera, directional backlight, exposure, and per-pixel seeds.
  DirectionalLight light(Vector3D(1),Vector3D(0,0,-1));
  Fixture f(Vector3D(0,0,1.1),.45,false,{&light});
  f.accel.total_rays=f.accel.total_isects=0;
  f.tracer.medium=std::make_shared<HomogeneousMedium>(BBox(Vector3D(-1,-1,0),Vector3D(1,1,2)),1,.8,.7);
  f.tracer.max_ray_depth=4;
  const unsigned width=80,height=80,spp=256;
  std::ofstream stats(out+"/render_summary.csv");
  stats<<"g,width,height,spp,depth,mean_linear_radiance\n";
  for(double g:{0.,.5,.8}) {
    f.tracer.phase=std::make_shared<HenyeyGreensteinPhase>(g);
    std::vector<unsigned char> pixels(width*height*4);double total=0;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
      RNG rng(184+y*width+x);Vector3D L(0);
      for(unsigned s=0;s<spp;++s){double px=2.4*(x+rng.uniform())/width-1.2,py=1.2-2.4*(y+rng.uniform())/height;auto sample=f.tracer.est_radiance_volume(Ray(Vector3D(px,py,-1),Vector3D(0,0,1)),rng);check(std::isfinite(sample.x)&&std::isfinite(sample.y)&&std::isfinite(sample.z)&&sample.x>=0&&sample.y>=0&&sample.z>=0,"valid rendered sample");L+=sample;}
      L/=spp;total+=L.x;
      for(unsigned c=0;c<3;++c){double mapped=std::pow(L[c]/(1+L[c]),1/2.2);pixels[4*(y*width+x)+c]=static_cast<unsigned char>(255*std::min(1.,mapped)+.5);}pixels[4*(y*width+x)+3]=255;
    }
    const std::string tag=g==0?"0":g==.5?"05":"08";
    check(lodepng::encode(out+"/fog_g"+tag+".png",pixels,width,height)==0,"save rendered PNG");
    stats<<g<<','<<width<<','<<height<<','<<spp<<",4,"<<total/(width*height)<<'\n';
  }
}
int main(int argc,char**argv) {
  if(part_a_regression_main()!=0)return 1;
  try {const std::string out=argc>1?argv[1]:".";hg_integrator_checks(out);render_hg_comparison(out);}
  catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
  std::cout<<"PASS: "<<checks<<" assertions including A regressions, integrated HG, and render sample validity\n";
}
