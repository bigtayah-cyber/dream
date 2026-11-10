// Rendu « Satin » d'Orbe Autotune : modulation parasite (3-20 Hz), justesse du centre, peigne avec Mix < 100 %, rendus Précis/Robot.
// Cible : satin_test (EXCLUDE_FROM_ALL), sans JUCE.
#include <cstdio>
#include <random>
#include <complex>
#include <string>
#include "../Source/Suite/OrbeAutotune/AutotuneDsp.h"
using V = std::vector<float>;
static const double SR = 44100.0;
static double formantG(double f){auto b=[&](double c,double w,double g){return g*std::exp(-std::pow((f-c)/w,2.0));};return 0.04+b(700,300,1.0)+b(1250,350,0.55)+b(2600,450,0.25);}
struct Note{double t0,t1;int n;double off;};
static std::vector<Note> notes;
// voix : tenues décalées de la note, vibrato 5,5 Hz ±30 ct, glissandi, consonnes, souffle
static V makeVoice(bool vib=true){
  std::mt19937 rg(7); std::normal_distribution<double> g(0,1);
  const int seq[5]={57,60,62,64,60}; const double offs[5]={-25,18,-12,30,-8};
  double T=0; notes.clear();
  for(int i=0;i<5;i++){notes.push_back({T,T+1.3,seq[i],offs[i]});T+=1.3+0.15;}
  size_t n=(size_t)(T*SR); V x(n,0.f); double ph=0, jit=0; 
  for(size_t i=0;i<n;i++){double t=i/SR; int k=-1; for(int j=0;j<5;j++) if(t>=notes[j].t0&&t<notes[j].t1) k=j;
    double m, amp=0; 
    if(k>=0){ double lt=t-notes[k].t0; m=notes[k].n+notes[k].off*0.01;
      if(vib) m+=0.30*std::sin(2*M_PI*5.5*lt)*std::min(1.0,std::max(0.0,(lt-0.25)/0.3));
      if(lt<0.08 && k>0){ double a=lt/0.08; double pm=notes[k-1].n+notes[k-1].off*0.01; m=pm+(m-pm)*(0.5-0.5*std::cos(M_PI*a)); } // glissando d'attaque
      amp=std::min(1.0,std::min(lt,notes[k].t1-t)/0.02);
    } else m=60;
    jit+=0.002*g(rg)-0.02*jit; m+=jit*0.03;   // micro-gigue ~3 ct
    double f0=440*std::pow(2.0,(m-69)/12.0); ph+=2*M_PI*f0/SR; double v=0;
    for(int h=1;h*f0<5000&&h<=60;h++) v+=formantG(h*f0)*std::sin(h*ph);
    double cons = (k<0)? 0.05*g(rg):0.0;   // consonne bruitée entre les notes
    x[i]=(float)(0.25*amp*v + cons + 0.004*g(rg));
  }
  return x;
}
static double rms(const V& x,size_t a,size_t b){double e=0;for(size_t i=a;i<b;i++)e+=x[i]*x[i];return std::sqrt(e/(b-a+1e-9));}
// indice de modulation d'amplitude 3–20 Hz (dB) sur [a,b)
static double amIdx(const V& x,size_t a,size_t b){
  const int B=128; std::vector<double> e; for(size_t i=a;i+B<=b;i+=B){double s=0;for(int j=0;j<B;j++)s+=x[i+j]*x[i+j];e.push_back(std::sqrt(s/B));}
  double mu=0; for(double v:e)mu+=v; mu/=e.size(); const double fr=SR/B; int N=e.size(); double p=0;
  for(int k=1;k<N/2;k++){double f=k*fr/N; if(f<3||f>20)continue; std::complex<double> c=0; for(int j=0;j<N;j++){double w=0.5-0.5*std::cos(2*M_PI*j/N); c+=w*(e[j]-mu)*std::polar(1.0,-2*M_PI*k*j/N);} p+=std::norm(c);} 
  p*=2.0/(N*N*0.375); return 10*std::log10(p/(mu*mu)+1e-12);
}
static V bandpass(const V& x,double lo,double hi){atn::Biquad h1,l1;l1.lowpass(SR,hi);V y(x.size());double z=0,a=std::exp(-2*M_PI*lo/SR);float p=0;
  for(size_t i=0;i<x.size();i++){float v=l1.process(x[i]); float hp=v-p; p=v; z=a*z+hp; y[i]=(float)(hp + 0*z);} // HP 1er ordre approximatif
  atn::Biquad l2; l2.lowpass(SR,hi); for(auto&v:y) v=l2.process(v);
  // passe-haut supplémentaire 2e ordre (forme simple)
  double y1=0,x1=0; double rc=1/(2*M_PI*lo), dt=1/SR, al=rc/(rc+dt); for(auto&v:y){double o=al*(y1+v-x1); x1=v; y1=o; v=(float)o;} return y;}
// f0 de sortie (cents vs cible) sur [a,b)
static double gCentre=0;
static void pitchDev(const V& x,size_t a,size_t b,double target,double& rmsC,double& hfC){
  const int N=2048,H=256; std::vector<double> c;
  const double P0=SR/(440*std::pow(2.0,(target-69)/12.0)); int lo=(int)(P0*0.9),hi=(int)(P0*1.1)+1;
  for(size_t i=a;i+N+hi<b;i+=H){ std::vector<double> r(hi+2); for(int L=lo-1;L<=hi+1;L++){double s=0,e1=0,e2=0;for(int j=0;j<N;j++){s+=x[i+j]*x[i+j+L];e1+=x[i+j]*x[i+j];e2+=x[i+j+L]*x[i+j+L];} r[L]=s/std::sqrt(e1*e2+1e-20);} 
    int bl=lo; for(int L=lo;L<=hi;L++) if(r[L]>r[bl])bl=L; double d=r[bl-1]-2*r[bl]+r[bl+1]; double t=bl+(d<0?0.5*(r[bl-1]-r[bl+1])/d:0);
    c.push_back(1200*std::log2(P0/t)); }
  double s=0,mu=0; for(double v:c){s+=v*v;mu+=v;} rmsC=std::sqrt(s/c.size()); gCentre+=std::fabs(mu/c.size());
  double h=0; int n=0; for(size_t i=4;i+4<c.size();i++){double m=0;for(int j=-4;j<=4;j++)m+=c[i+j];m/=9;h+=(c[i]-m)*(c[i]-m);n++;} hfC=std::sqrt(h/std::max(1,n));
}
struct Res{double amB,amH,pr,ph,thd;};
static V run(atn::Params p,const V& x,int bs=512){atn::Engine e; e.prepare(SR); e.set(p); e.set(p); V l=x,r=x; for(size_t i=0;i<x.size();i+=bs){int n=(int)std::min<size_t>(bs,x.size()-i); e.process(&l[i],&r[i],n);} 
  int lat=e.latency(); V o(x.size(),0.f); for(size_t i=0;i+lat<x.size();i++) o[i]=0.5f*(l[i+lat]+r[i+lat]); return o;}
static Res measure(const V& o){Res R{0,0,0,0,0}; gCentre=0; V hb=bandpass(o,1500,5000); int k=0;
  for(auto&nt:notes){ size_t a=(size_t)((nt.t0+0.45)*SR), b=(size_t)((nt.t1-0.1)*SR); R.amB+=amIdx(o,a,b); R.amH+=amIdx(hb,a,b); double pr,ph; pitchDev(o,a,b,nt.n,pr,ph); R.pr+=pr; R.ph+=ph; k++;}
  R.amB/=k;R.amH/=k;R.pr/=k;R.ph/=k; return R;}
static double thdn(atn::Params p){ // sinus juste (A4) : tout ce qui n'est pas la fondamentale
  V x((size_t)(2*SR)); for(size_t i=0;i<x.size();i++) x[i]=(float)(0.2*std::sin(2*M_PI*440*i/SR)*std::min(1.0,i/(0.02*SR))); V o=run(p,x);
  const int N=16384; size_t a=(size_t)(1.2*SR); std::vector<std::complex<double>> X(N); double tot=0,fund=0;
  for(int k=1;k<N/2;k++){std::complex<double> c=0; if(k%1)continue; } // (évité : DFT complète trop lente) -> Goertzel autour de la fondamentale
  // énergie totale et énergie de la meilleure sinusoïde 440 Hz (ajustement moindres carrés)
  double sc=0,ss=0,cc=0,s2=0,cs=0,e=0; for(int j=0;j<N;j++){double w=2*M_PI*440*(a+j)/SR; double s=std::sin(w),c=std::cos(w),v=o[a+j]; ss+=s*s;cc+=c*c;cs+=s*c;sc+=v*s;s2+=v*c;e+=v*v;}
  double det=ss*cc-cs*cs, A=(sc*cc-s2*cs)/det, Bc=(s2*ss-sc*cs)/det; double res=0; for(int j=0;j<N;j++){double w=2*M_PI*440*(a+j)/SR; double r=o[a+j]-A*std::sin(w)-Bc*std::cos(w); res+=r*r;}
  (void)tot;(void)fund; return 10*std::log10(res/e+1e-15);
}

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("  ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
int main() {
  const V x = makeVoice(true);
  atn::Params d; d.key = 0; d.scale = 0; d.orbe = true; d.lowLat = true;   // défauts du plugin (Orbe, Direct, doublage .3), tonalité imposée
  auto sat = d; sat.render = 0; sat.dbl = 0.f;                              // nouvelle instance : Satin, sans doublage
  const Res r0 = measure(run(d, x)); const double c0 = gCentre / 5;
  for (int lat = 1; lat >= 0; --lat) {
    auto s = sat; s.lowLat = lat == 1;
    const Res r1 = measure(run(s, x)); const double c1 = gCentre / 5;
    std::printf("  %s : AM 3-20 Hz %.1f dB (actuel %.1f) | 1,5-5 kHz %.1f dB (actuel %.1f) | centre %.1f ct (actuel %.1f)\n", lat ? "Direct" : "Qualite", r1.amB, r0.amB, r1.amH, r0.amH, c1, c0);
    CHECK(r1.amB <= r0.amB - 10.0, "Satin : modulation 3-20 Hz pas assez reduite (%.1f vs %.1f dB)", r1.amB, r0.amB);
    CHECK(r1.amH <= r0.amH - 6.0, "Satin : modulation 1,5-5 kHz (%.1f vs %.1f dB)", r1.amH, r0.amH);
    CHECK(c1 < 3.0, "Satin : centre de note faux (%.1f ct)", c1);
  }
  { auto s = sat; s.mix = 0.5f; const Res r = measure(run(s, x)); auto p = d; p.mix = 0.5f; const Res q = measure(run(p, x));
    std::printf("  Mix 50 %% : Satin %.1f dB, Precis %.1f dB (peigne)\n", r.amB, q.amB);
    CHECK(r.amB <= q.amB - 15.0, "Satin mix 50 %% : peigne (%.1f vs %.1f dB)", r.amB, q.amB); }
  { // Robot = Précis vitesse 0 tolérance 0, à l'échantillon près ; Précis = comportement d'origine (render par défaut des Params)
    auto rb = d; rb.render = 2; auto p0 = d; p0.speed = 0; p0.tol = 0; const V a = run(rb, x), b = run(p0, x); double e = 0; for (size_t i = 0; i < a.size(); ++i) e = std::max(e, (double)std::fabs(a[i] - b[i]));
    CHECK(e == 0.0, "Robot != vitesse 0 (%.3g)", e);
    CHECK(atn::Params{}.render == 1, "Params par defaut doit rester Precis"); }
  { // tenue juste et sans vibrato : Satin n'ajoute pas de distorsion
    V s((size_t)(2 * SR)); for (size_t i = 0; i < s.size(); ++i) s[i] = (float)(0.2 * std::sin(2 * M_PI * 440 * i / SR) * std::min(1.0, i / (0.02 * SR)));
    (void)s; const double t = thdn(sat); std::printf("  THD+N sinus juste (Satin) : %.1f dB\n", t); CHECK(t < -80.0, "THD+N Satin %.1f dB", t); }
  std::printf(fails ? "\n%d ECHEC(S)\n" : "\ntout est bon\n", fails);
  return fails ? 1 : 0;
}
