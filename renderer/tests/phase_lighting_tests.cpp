#include "volume/hg_phase.h"
#include "volume/direct_lighting.h"
#include "scene/light.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
using namespace CGL;
using namespace CGL::Volume;
using namespace CGL::SceneObjects;
namespace {
size_t checks=0;
void check(bool ok,const char* msg){++checks;if(!ok)throw std::runtime_error(msg);}
void near(double x,double y,double tol,const char* msg){check(std::isfinite(x)&&std::abs(x-y)<=tol,msg);}
template<class F> void rejects(F f){bool yes=false;try{f();}catch(const std::exception&){yes=true;}check(yes,"invalid input must throw");}
// Explicit fixture primitive; uses the repository's BBox intersection and BVH.
struct BoxFixture : Primitive {
 BBox box; explicit BoxFixture(double z):box(Vector3D(-1,-1,z),Vector3D(1,1,z+.1)){}
 BBox get_bbox()const override{return box;}
 bool has_intersection(const Ray&r)const override{double a=r.min_t,b=r.max_t;return box.intersect(r,a,b);}
 bool intersect(const Ray&r,Intersection*)const override{return has_intersection(r);}
 BSDF* get_bsdf()const override{return nullptr;}
 void draw(const Color&,float)const override{}
 void drawOutline(const Color&,float)const override{}
};
// Analytic illumination fixture, not a production environment-light implementation.
struct UniformSphereFixture : SceneLight {
 mutable RNG random{91};
 Vector3D sample_L(Vector3D,Vector3D*wi,double*d,double*p)const override{
  IsotropicPhase iso;*wi=iso.sample(Vector3D(0,0,1),random).direction;*d=INF_D;*p=1/(4*PI);return Vector3D(1);
 }
 bool is_delta_light()const override{return false;}
};
void phase_tests(const std::string& out){
 std::ofstream summary(out+"/phase_summary.csv"),hist(out+"/phase_histogram.csv");
 check(bool(summary)&&bool(hist),"output directory must exist");
 summary<<std::setprecision(17)<<"g,n,mean_mu,expected_mu,standard_error,mean_mu2,expected_mu2\n";
 hist<<std::setprecision(17)<<"g,mu_left,mu_right,count,n,expected_probability\n";
 const int n=200000,bins=80;
 const Vector3D w=Vector3D(1,2,3).unit();
 for(double g:{-.8,-.5,-1e-9,0.,1e-9,.5,.8}){
  HenyeyGreensteinPhase phase(g);RNG rng(184);double sum=0,sq=0,tx=0,ty=0;int counts[bins]={};
  Vector3D u=cross(Vector3D(0,0,1),w).unit(),v=cross(w,u);
  for(int i=0;i<n;++i){auto s=phase.sample(w,rng);near(s.direction.norm(),1,2e-14,"unit sample");check(s.pdf>0&&std::isfinite(s.pdf),"positive finite pdf");near(s.value/s.pdf,1,1e-14,"phase/PDF");near(s.pdf,phase.evaluate(w,s.direction),1e-12,"evaluation consistency");double mu=dot(w,s.direction);sum+=mu;sq+=mu*mu;tx+=dot(u,s.direction);ty+=dot(v,s.direction);++counts[std::min(bins-1,std::max(0,int((mu+1)*bins/2)))];}
  double mean=sum/n,second=sq/n,se=std::sqrt((second-mean*mean)/(n-1));
  near(mean,g,5*se+1e-10,"mean cosine agrees within 5 SE");
  near(second,(1+2*g*g)/3,0.008,"second moment");
  near(tx/n,0,.008,"azimuth x symmetry");near(ty/n,0,.008,"azimuth y symmetry");
  summary<<g<<','<<n<<','<<mean<<','<<g<<','<<se<<','<<second<<','<<(1+2*g*g)/3<<'\n';
  double total=0;
  for(int b=0;b<bins;++b){double lo=-1+2.*b/bins,hi=lo+2./bins;double probability=0; // independent numerical integration
   for(int k=0;k<100;++k){double mu=lo+(k+.5)*(hi-lo)/100;probability+=(hi-lo)/100*(1-g*g)/(2*std::pow(1+g*g-2*g*mu,1.5));}
   total+=probability;double sebin=std::sqrt(probability*(1-probability)/n);near(double(counts[b])/n,probability,6*sebin+2e-5,"histogram shape");hist<<g<<','<<lo<<','<<hi<<','<<counts[b]<<','<<n<<','<<probability<<'\n';}
  near(total,1,1e-5,"PDF integrates to one");
 }
 for(double g:{-.999999,.999999,0.,1e-5,-1e-5})for(Vector3D w0:{Vector3D(0,0,1),Vector3D(0,0,-1),Vector3D(1,0,0),Vector3D(1e-15,1,1e-15)}){HenyeyGreensteinPhase p(g);RNG r(9);for(int i=0;i<1000;++i){auto s=p.sample(w0,r);near(s.direction.norm(),1,2e-14,"pole frame");check(std::isfinite(s.pdf)&&s.pdf>0,"extreme-g PDF");}}
 for(double g:{-1.,1.,2.,std::numeric_limits<double>::quiet_NaN(),INF_D})rejects([&](){HenyeyGreensteinPhase p(g);});
 HenyeyGreensteinPhase p(.5);RNG r(4);rejects([&](){p.sample(Vector3D(0),r);});rejects([&](){p.evaluate(Vector3D(INF_D,0,0),Vector3D(1));});
 check(p.evaluate(Vector3D(0,0,1),Vector3D(0,0,1))>p.evaluate(Vector3D(0,0,1),Vector3D(0,0,-1)),"positive g favors forward");
 RNG r1(1),r2(1);for(int i=0;i<100;++i)near((p.sample(w,r1).direction-p.sample(w,r2).direction).norm(),0,0,"repeatable seed");
}
void lighting_tests(const std::string&out){
 BVHAccel empty(std::vector<Primitive*>{});empty.total_rays=empty.total_isects=0;
 RNG rng(184);Vector3D origin(0),w(0,0,1);HenyeyGreensteinPhase phase(.5);IsotropicPhase iso;
 PointLight point(Vector3D(2,3,4),Vector3D(0,0,2));std::vector<SceneLight*> lights{&point};
 auto clear=[](const Ray&,RNG&){return 1.;};
 auto estimate=[&](const BVHAccel&b){return direct_volume_lighting(origin,w,phase,lights,b,4,rng,clear);};
 near(estimate(empty).x,2*phase.evaluate(w,w),1e-12,"point light convention and phase sign");
 lights.push_back(&point);near(estimate(empty).x,4*phase.evaluate(w,w),1e-12,"sum lights without selection PDF");lights.pop_back();
 BoxFixture blocker(.8),behind(3);BVHAccel blocked({&blocker}),beyond({&behind});blocked.total_rays=blocked.total_isects=beyond.total_rays=beyond.total_isects=0;
 near(estimate(blocked).norm(),0,0,"opaque blocker");near(estimate(beyond).x,estimate(empty).x,0,"blocker beyond light ignored");
 HomogeneousMedium medium(BBox(Vector3D(-1,-1,.5),Vector3D(1,1,1.5)),1,.8,1);
 auto tr=[&](const Ray&r,RNG&random){Interval i;return medium.clip(r,r.max_t,i)?medium.transmittance(r,i,random):1.;};
 auto attenuated=direct_volume_lighting(origin,w,phase,lights,empty,1,rng,tr);
 near(attenuated.x,estimate(empty).x*std::exp(-.8),1e-12,"clipped analytic attenuation");
 // A perpendicular light has nonzero volume scattering: no surface cosine.
 PointLight side(Vector3D(1),Vector3D(2,0,0));lights={&side};
 near(direct_volume_lighting(origin,w,iso,lights,empty,1,rng,clear).x,1/(4*PI),1e-14,"no surface cosine");
 rejects([&](){direct_volume_lighting(origin,w,iso,lights,empty,0,rng,clear);});
 rejects([&](){direct_volume_lighting(origin,w,iso,lights,empty,1,rng,[](const Ray&,RNG&){return -1.;});});
 std::ofstream report(out+"/lighting_summary.csv");report<<std::setprecision(17)<<"test,n,estimate,reference,standard_error\n";
 UniformSphereFixture uniform;lights={&uniform};
 for(double g:{0.,.5,.8}){HenyeyGreensteinPhase p(g);const int n=100000;double sum=0,sq=0;for(int i=0;i<n;++i){double x=direct_volume_lighting(origin,w,p,lights,empty,1,rng,clear).x;sum+=x;sq+=x*x;}double mean=sum/n,se=std::sqrt(std::max(0.,sq/n-mean*mean)/(n-1));near(mean,1,5*se+1e-12,"uniform illumination integral");report<<"uniform_sphere_g_"<<g<<','<<n<<','<<mean<<",1,"<<se<<'\n';}
 // Actual repository AreaLight; centered 2x2 rectangle at distance 2.
 AreaLight area(Vector3D(1),Vector3D(0,0,2),Vector3D(0,0,-1),Vector3D(2,0,0),Vector3D(0,2,0));lights={&area};
 const int n=100000;double sum=0,sq=0;for(int i=0;i<n;++i){double x=direct_volume_lighting(origin,w,iso,lights,empty,1,rng,clear).x;sum+=x;sq+=x*x;}
 double reference=4*std::atan(1/(2*std::sqrt(6.)))/(4*PI),mean=sum/n,se=std::sqrt((sq/n-mean*mean)/(n-1));near(mean,reference,5*se,"real area light versus rectangle solid angle");report<<"area_rectangle,"<<n<<','<<mean<<','<<reference<<','<<se<<'\n';
}
}
int main(int argc,char**argv){try{std::string out=argc>1?argv[1]:".";phase_tests(out);lighting_tests(out);std::cout<<"PASS: "<<checks<<" checks; phase and lighting CSVs saved to "<<out<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
