// Orbe · analyse partagée (C++17 pur, sans JUCE, testable hors-ligne).
// Portage fidèle des prototypes detect.html / mix.html / fredonne.html. Les textes sont en UTF-8.
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace oa {

constexpr double kPi = 3.14159265358979323846;
inline double jsRound(double v) { return std::floor(v + 0.5); }
inline double db10(double x) { return 10.0 * std::log10(x > 1e-20 ? x : 1e-20); }
inline double mtof(double n) { return 440.0 * std::pow(2.0, (n - 69.0) / 12.0); }
inline double ftom(double f) { return 69.0 + 12.0 * std::log2(f / 440.0); }
inline std::string n1(double v);          // « 12,3 »
inline std::string f1(double v);          // « +1,2 »

extern const char* const kNoteNames[12];  // C C♯ D E♭ E F F♯ G A♭ A B♭ B
extern const int kMajor[7], kMinor[7];

void fft(double* re, double* im, int n);  // radix-2 en place, n puissance de 2

// ---------------- tempo / tonalité ----------------
struct Tempo { double bpm = 0, conf = 0, phase = 0, alt[2] = {0, 0}; int nAlt = 0; };
Tempo detectTempo(const float* mono, int n, double sr);

struct KeyRank { int pc = 0; bool minor = false; double r = 0; };
struct Key { int pc = 0; bool minor = false; double r = 0, conf = 0; double chroma[12] = {}; KeyRank rank[4]; };
Key detectKey(const float* mono, int n, double sr);

std::vector<double> onsets(const float* mono, int n, double sr, double sens = 0.5, int maxN = 16);

std::string camelot(int pc, bool minor);                       // « 8A »
bool fromCamelot(const std::string& code, int& pc, bool& minor);
std::array<std::string, 4> compatCodes(const std::string& code);
inline double camelotHue(int number) { return std::fmod(number * 30.0 + 160.0, 360.0); }
std::string keyShort(int pc, bool minor);                      // « Am »
std::string keyLong(int pc, bool minor);                       // « A mineur »

// ---------------- mesure du mix ----------------
struct Band { const char* k; const char* label; double lo, hi, tol; };
extern const Band kBands[7];
struct Profile { const char* id; const char* group; const char* label; double lufsLo, lufsHi, plr; double tilt[7]; };
extern const Profile kProfiles[11];
constexpr int kCurveN = 61;               // courbe 1/6 d'octave, 20 Hz → 20 kHz
double curveFreq(int i);

struct MixSnap {                          // POD publié par le fil audio
    int ch = 2, clips = 0, frames = 0;
    double seconds = 0;
    float lufsM = -70, lufsS = -70, lufsI = -70, lra = 0, tp = -100, peak = -100, plr = 0, corr = 1, corrLive = 1, dc = 0;
    float lowSide = -60, totSide = -60;
    float bands[7] = {}, curve[kCurveN] = {}, live[kCurveN] = {};
    bool valid = false;
};

class MixMeter {
public:
    void prepare(double sr);
    void reset();
    void process(const float* L, const float* R, int n);   // R == L si mono
    void fill(MixSnap& s) const;
    double seconds() const { return double(total) / fs; }
private:
    struct Bq { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        double run(double v) { const double o = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = v; y2 = y1; y1 = o; return o; } };
    struct Hist { std::vector<double> sum; std::vector<uint32_t> cnt; void init() { sum.assign(8001, 0.0); cnt.assign(8001, 0); } void clear() { std::fill(sum.begin(), sum.end(), 0.0); std::fill(cnt.begin(), cnt.end(), 0u); } };
    void addSpectrum();
    double fs = 48000;
    int seg = 4800, segFill = 0, segCount = 0;
    double aSum = 0, bSum = 0, segP[30] = {};
    Bq kA[2], kB[2];
    bool mono = false;
    Hist hi, hs;
    double hx[2][4] = {};
    double peakLin = 0, tpLin = 0, sL = 0, sR = 0, LL = 0, RR = 0, LR = 0, cl = 0, cr = 0, clr = 0;
    int run = 0, clips = 0;
    long long total = 0;
    int ring = 0;
    std::vector<double> rm, rs, pm, ps, win, re, im, tmp;
    mutable std::vector<double> sa, sb;
    std::vector<float> live;
    int nFrames = 0;
    double lastNorm = 0;
    friend struct MixAccess;
};

struct Diag { int sev = 2; int band = -1; std::string title, meas, fix, group; };   // sev : 0 à corriger, 1 à surveiller, 2 ok
struct Report { std::vector<Diag> diags; int score = 100; double shape[7] = {}, target[7] = {}, tol[7] = {}, dev[7] = {}; double lufsLo = 0, lufsHi = 0, plr = 0; };
Report evaluate(const MixSnap& A, int profileIndex);

// ---------------- hauteur / segmentation en notes ----------------
struct PitchFrame { float t = 0, rms = 0, f = 0, q = 1; };
struct PitchTrack { std::vector<PitchFrame> frames; float maxR = 0; double dur = 0; };
PitchTrack analyzePitch(const float* mono, int n, double sr);

struct SegParams { double sens = 0.5, minlen = 0.09, bpm = 100; int grid = 1, oct = 0, key = -1, mode = -1; bool tune = true, snap = true; };  // mode 0 min, 1 maj, 2 chromatique
struct Note { double s = 0, e = 0, raw = 0; int n = 0; };
struct SegResult { std::vector<Note> notes; int key = 9, mode = 0, autoKey = 9, autoMode = 0; double cents = 0; };
SegResult segment(const PitchTrack&, const SegParams&);
struct MidiNote { double t, d; int n; double v; };      // t, d en temps (noires)
std::vector<uint8_t> writeMidi(const std::vector<MidiNote>& notes, double bpm, const char* trackName = "Fredonne");

// Publication sans verrou d'une structure POD du fil audio vers l'interface.
template <class T> class SeqPub {
public:
    void write(const T& v) { const unsigned s = seq.load(std::memory_order_relaxed); seq.store(s + 1, std::memory_order_release); std::atomic_thread_fence(std::memory_order_release); data = v; std::atomic_thread_fence(std::memory_order_release); seq.store(s + 2, std::memory_order_release); }
    T read() const { T o; for (int i = 0; i < 8; ++i) { const unsigned a = seq.load(std::memory_order_acquire); if (a & 1u) continue; std::atomic_thread_fence(std::memory_order_acquire); o = data; std::atomic_thread_fence(std::memory_order_acquire); if (seq.load(std::memory_order_acquire) == a) return o; } return o; }
private:
    std::atomic<unsigned> seq{0};
    T data{};
};

inline std::string n1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); for (char* p = b; *p; ++p) if (*p == '.') *p = ','; return b; }
inline std::string f1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); for (char* p = b; *p; ++p) if (*p == '.') *p = ','; return (v > 0 ? "+" : "") + std::string(b); }

}  // namespace oa
