// Moteur d'Orbe Galet : mise en forme par le temps. Une forme périodique dessinée (points + courbure) donne une valeur
// m ∈ [0,1] (0 = neutre, 1 = effet plein) qui pilote quatre modules : Volume, Filtre (états-variables TPT), Distorsion lisse
// (ADAA du 1er ordre, sur-échantillonnage ×4 à phase linéaire), Pan/Largeur. Chaîne : Filtre → Volume → Pan → Distorsion.
// Tout est lissé (forme, profondeurs, réglages, changements de forme en fondu). Sans JUCE, sans allocation en temps réel.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace gal {
constexpr double kPi = 3.14159265358979323846;
constexpr int kMaxPts = 32, kLut = 2048;

// ------------------------------------------------------------------ forme
struct Point { float x = 0, y = 0, c = 0; };   // x ∈ [0,1), y ∈ [0,1], c ∈ [-1,1] : courbure du segment qui part de ce point
struct Shape {
    int n = 1; Point p[kMaxPts];
    bool operator==(const Shape& o) const {
        if (n != o.n) return false;
        for (int i = 0; i < n; ++i) if (p[i].x != o.p[i].x || p[i].y != o.p[i].y || p[i].c != o.p[i].c) return false;
        return true;
    }
};
enum Interp { Free = 0, Sym = 1, Spline = 2 };

// Remise en ordre : bornes, tri par x, premier point en x = 0.
inline void sanitize(Shape& s) {
    s.n = std::clamp(s.n, 1, kMaxPts);
    for (int i = 0; i < s.n; ++i) {
        auto& q = s.p[i];
        if (!std::isfinite(q.x)) q.x = 0;
        if (!std::isfinite(q.y)) q.y = 0;
        if (!std::isfinite(q.c)) q.c = 0;
        q.x = std::clamp(q.x, 0.f, 0.999f); q.y = std::clamp(q.y, 0.f, 1.f); q.c = std::clamp(q.c, -1.f, 1.f);
    }
    for (int i = 1; i < s.n; ++i) for (int j = i; j > 0 && s.p[j - 1].x > s.p[j].x; --j) std::swap(s.p[j - 1], s.p[j]);   // tri stable sans allocation
    s.p[0].x = 0.f;
}

// Courbure d'un segment : exponentielle (libre) ou en S (symétrique), t ∈ [0,1].
inline float bend(float t, float c, bool sym) {
    auto ex = [](float u, float a) { return std::fabs(a) < 1e-3f ? u : (std::exp(a * u) - 1.f) / (std::exp(a) - 1.f); };
    const float a = c * 7.f;
    if (!sym) return ex(t, a);
    return t < 0.5f ? 0.5f * ex(2.f * t, a) : 1.f - 0.5f * ex(2.f - 2.f * t, a);
}

// Tabule la forme (période complète, kLut+1 valeurs, la dernière = la première). Sans allocation.
inline void render(const Shape& s, int interp, float* lut) {
    float xs[kMaxPts + 1], ys[kMaxPts + 1], cs[kMaxPts + 1], m[kMaxPts + 1];
    const int n = std::clamp(s.n, 1, kMaxPts);
    for (int i = 0; i < n; ++i) { xs[i] = s.p[i].x; ys[i] = s.p[i].y; cs[i] = s.p[i].c; }
    xs[n] = 1.f; ys[n] = ys[0]; cs[n] = 0.f;
    if (interp == Spline) {   // Hermite monotone périodique (Fritsch–Butland) : lisse, sans dépassement
        float d[kMaxPts + 1], h[kMaxPts + 1];
        for (int k = 0; k < n; ++k) { h[k] = xs[k + 1] - xs[k]; d[k] = h[k] > 1e-6f ? (ys[k + 1] - ys[k]) / h[k] : 0.f; }
        for (int k = 0; k <= n; ++k) {
            const int a = k == 0 || k == n ? n - 1 : k - 1, b = k == n ? 0 : k;
            const float d0 = d[a], d1 = d[b], h0 = h[a], h1 = h[b];
            m[k] = (d0 * d1 <= 0.f || h0 <= 1e-6f || h1 <= 1e-6f) ? 0.f : 3.f * (h0 + h1) / ((2.f * h1 + h0) / d0 + (h1 + 2.f * h0) / d1);
        }
    }
    int k = 0;
    for (int i = 0; i <= kLut; ++i) {
        const float x = i == kLut ? 0.f : (float)i / (float)kLut;
        while (k < n - 1 && x >= xs[k + 1]) ++k;
        const float w = xs[k + 1] - xs[k], t = w > 1e-6f ? std::clamp((x - xs[k]) / w, 0.f, 1.f) : 1.f;
        float y;
        if (interp == Spline) {
            const float t2 = t * t, t3 = t2 * t;
            y = (2 * t3 - 3 * t2 + 1) * ys[k] + (t3 - 2 * t2 + t) * w * m[k] + (-2 * t3 + 3 * t2) * ys[k + 1] + (t3 - t2) * w * m[k + 1];
        } else y = ys[k] + (ys[k + 1] - ys[k]) * bend(t, cs[k], interp == Sym);
        lut[i] = std::clamp(y, 0.f, 1.f);
        if (i == kLut) lut[i] = lut[0];
    }
}

// Texte d'état versionné : « G1;n;x,y,c;… ». Lecture tolérante (forme par défaut si illisible).
inline std::string toString(const Shape& s) {
    std::string o = "G1;" + std::to_string(s.n);
    char b[64];
    for (int i = 0; i < s.n; ++i) { std::snprintf(b, sizeof b, ";%.5f,%.5f,%.4f", s.p[i].x, s.p[i].y, s.p[i].c); o += b; }
    return o;
}
inline bool fromString(const std::string& str, Shape& out) {
    if (str.rfind("G1;", 0) != 0) return false;
    const char* c = str.c_str() + 3; char* e = nullptr;
    const long n = std::strtol(c, &e, 10); if (e == c || n < 1 || n > kMaxPts) return false;
    Shape s; s.n = (int)n; c = e;
    for (int i = 0; i < s.n; ++i) {
        float v[3];
        for (int j = 0; j < 3; ++j) { if (*c != (j ? ',' : ';')) return false; ++c; v[j] = std::strtof(c, &e); if (e == c) return false; c = e; }
        s.p[i] = {v[0], v[1], v[2]};
    }
    sanitize(s); out = s; return true;
}

// Formes de départ (8). Interp conseillée renvoyée dans `interp`.
inline constexpr int kNumStarts = 8;
inline const char* const kStartNames[kNumStarts] = {"Sinus", "Triangle", "Pompe", "Mont\xC3\xA9" "e", "Descente", "Porte", "Escalier", "Rebond"};
inline Shape startShape(int idx, int* interp = nullptr) {
    Shape s; int ip = Free;
    auto set = [&s](std::initializer_list<Point> l) { s.n = 0; for (auto& q : l) s.p[s.n++] = q; };
    switch (idx) {
        case 0: set({{0, 0, 0.32f}, {0.5f, 1, 0.32f}}); ip = Sym; break;
        case 1: set({{0, 0, 0}, {0.5f, 1, 0}}); break;
        case 2: set({{0, 1, -0.62f}, {0.55f, 0, 0}, {0.965f, 0, 0.3f}}); break;
        case 3: set({{0, 0, 0}, {0.97f, 1, 0}}); break;
        case 4: set({{0, 1, 0}, {0.97f, 0, 0}}); break;
        case 5: set({{0, 1, 0}, {0.47f, 1, 0}, {0.5f, 0, 0}, {0.97f, 0, 0}}); break;
        case 6: set({{0, 0, 0}, {0.235f, 0, 0}, {0.25f, 0.333f, 0}, {0.485f, 0.333f, 0}, {0.5f, 0.667f, 0}, {0.735f, 0.667f, 0}, {0.75f, 1, 0}, {0.985f, 1, 0}}); break;
        default: set({{0, 1, -0.6f}, {0.26f, 0, 0}, {0.28f, 0.55f, -0.6f}, {0.5f, 0, 0}, {0.52f, 0.3f, -0.6f}, {0.7f, 0, 0}, {0.72f, 0.12f, -0.6f}, {0.86f, 0, 0}, {0.98f, 0, 0}}); break;
    }
    sanitize(s); if (interp) *interp = ip; return s;
}
// Motif à pas (« x » = effet plein, « . » = neutre), pas fusionnés ; front de 4 % du pas.
inline Shape stepShape(const char* pat) {
    Shape s; s.n = 0; const int N = (int)std::strlen(pat);
    for (int i = 0; i < N && s.n <= kMaxPts - 2; ++i) {
        const float v = pat[i] == 'x' ? 1.f : 0.f;
        if (s.n >= 2 && s.p[s.n - 1].y == v) { s.p[s.n - 1].x = ((float)i + 0.96f) / (float)N; continue; }
        s.p[s.n++] = {(float)i / (float)N, v, 0}; s.p[s.n++] = {((float)i + 0.96f) / (float)N, v, 0};
    }
    sanitize(s); return s;
}

// ------------------------------------------------------------------ réglages
struct Params {
    bool sync = true; int div = 4, feel = 0;     // division : 4 mes., 2 mes., 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 ; droit / triolet / pointé
    float hz = 1.f, phase = 0.f;                 // Hz libre ; décalage de phase (degrés)
    int retrig = 0;                              // 0 hôte (position PPQ), 1 note MIDI, 2 libre
    float smoothMs = 4.f; int interp = Free;
    bool volOn = true; float volDepth = 0.85f;
    bool fltOn = false; int fltMode = 0; float fltFreq = 400.f, fltRes = 0.3f, fltDepth = 1.f;   // passe-bas / bande / haut
    bool drvOn = false; int drvType = 0; float drive = 18.f, color = 0.f, drvDepth = 1.f;        // tanh / arctan / pli doux
    bool panOn = false; float pan = 0.f, width = 0.f;
    float mix = 1.f, outDb = 0.f;
};
struct Host { bool playing = false, hasPos = false, hasBpm = false; double ppq = 0, bpm = 120; };
inline double cycleQuarters(int div, int feel) {
    static const double q[8] = {16, 8, 4, 2, 1, 0.5, 0.25, 0.125};
    return q[std::clamp(div, 0, 7)] * (feel == 1 ? 2.0 / 3.0 : feel == 2 ? 1.5 : 1.0);
}

// ------------------------------------------------------------------ distorsion : fonctions et primitives (ADAA)
inline double shp(int type, double u) { return type == 0 ? std::tanh(u) : type == 1 ? std::atan(u) : std::sin(u); }
inline double prim(int type, double u) {
    if (type == 0) { const double a = std::fabs(u); return a + std::log1p(std::exp(-2.0 * a)) - 0.69314718055994531; }
    if (type == 1) return u * std::atan(u) - 0.5 * std::log1p(u * u);
    return -std::cos(u);
}
inline double foldGain(double g) { return std::sqrt(g); }   // le pli doux reçoit la moitié du drive (en dB)
// Gain de compensation : un signal de 0,5 crête ressort à 0,5 crête quel que soit le drive.
inline double compensation(int type, double g, double b) {
    constexpr double r = 0.5;
    if (type == 2) return r / std::sin(std::min(foldGain(g) * r, kPi / 2));
    const double d = shp(type, g * r + b) - shp(type, -g * r + b);
    return d > 1e-9 ? 2.0 * r / d : 1.0 / std::max(1e-9, g);
}
// Courbe de transfert statique (affichage) : x ∈ [-1,1] → sortie.
inline float transfer(int type, float driveDb, float color, float x) {
    const double g = std::pow(10.0, driveDb / 20.0), b = color * 0.6, ge = type == 2 ? foldGain(g) : g;
    return (float)((shp(type, ge * x + b) - shp(type, b)) * compensation(type, g, b));
}

// Filtre RIF demi-bande (fenêtre de Kaiser), longueur L = 4k+3 : centre impair → une phase = un seul coefficient (0,5).
inline std::vector<float> halfband(int L, double beta) {
    std::vector<float> h((size_t)L); const int c = (L - 1) / 2;
    auto i0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
    for (int n = 0; n < L; ++n) {
        const int k = n - c; const double r = (double)k / c;
        const double w = i0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / i0(beta);
        h[(size_t)n] = (float)((k == 0 ? 0.5 : std::sin(kPi * k / 2.0) / (kPi * k)) * w);
    }
    return h;
}
struct HalfUp {   // ×2 : sortie paire = taps pairs, sortie impaire = retard pur (c-1)/2
    std::vector<float> he, buf; int K = 0, D = 0, pos = 0;
    void init(const std::vector<float>& h) { const int L = (int)h.size(); K = (L + 1) / 2; D = ((L - 1) / 2 - 1) / 2; he.resize((size_t)K); for (int k = 0; k < K; ++k) he[(size_t)k] = 2.f * h[(size_t)(2 * k)]; buf.assign((size_t)(2 * K), 0.f); pos = 0; }
    void reset() { std::fill(buf.begin(), buf.end(), 0.f); pos = 0; }
    inline void push(float x, float& y0, float& y1) {
        pos = pos + 1 == K ? 0 : pos + 1; buf[(size_t)pos] = buf[(size_t)(pos + K)] = x;
        const float* b = buf.data() + pos + K; float s = 0.f;
        for (int k = 0; k < K; ++k) s += he[(size_t)k] * b[-k];
        y0 = s; y1 = b[-D];
    }
};
struct HalfDown {   // ÷2 : y = Σ h[2k]·pair[m-k] + 0,5·impair[m-(c+1)/2]
    std::vector<float> he, be, bo; int K = 0, D = 0, pe = 0, po = 0;
    void init(const std::vector<float>& h) { const int L = (int)h.size(); K = (L + 1) / 2; D = ((L - 1) / 2 + 1) / 2; he.resize((size_t)K); for (int k = 0; k < K; ++k) he[(size_t)k] = h[(size_t)(2 * k)]; be.assign((size_t)(2 * K), 0.f); bo.assign((size_t)(2 * (D + 1)), 0.f); pe = po = 0; }
    void reset() { std::fill(be.begin(), be.end(), 0.f); std::fill(bo.begin(), bo.end(), 0.f); pe = po = 0; }
    inline float push(float v0, float v1) {
        pe = pe + 1 == K ? 0 : pe + 1; be[(size_t)pe] = be[(size_t)(pe + K)] = v0;
        const float* b = be.data() + pe + K; float s = 0.f;
        for (int k = 0; k < K; ++k) s += he[(size_t)k] * b[-k];
        const int M = D + 1;
        po = po + 1 == M ? 0 : po + 1; bo[(size_t)po] = bo[(size_t)(po + M)] = v1;
        return s + 0.5f * bo[(size_t)(po + M - D)];   // impair d'il y a D pas
    }
};

// ------------------------------------------------------------------ moteur
class Engine {
public:
    static constexpr int kL1 = 127, kL2 = 31, kExtra4x = 2;
    // latence : (2·(L1-1) + (L2-1) + 0,5 ADAA + extra + (L2-1) + 2·(L1-1)) / 4 = 71,125 → 71 échantillons
    static constexpr int kLatency = (2 * (kL1 - 1) + (kL2 - 1) + kExtra4x) / 4, kUp = ((kL1 - 1) + (kL2 - 1) / 2) / 4;

    void prepare(double sampleRate, int maxBlock) {
        sr = sampleRate; (void)maxBlock;
        const auto h1 = halfband(kL1, 10.0), h2 = halfband(kL2, 10.0);
        for (int c = 0; c < 2; ++c) { up1[c].init(h1); up2[c].init(h2); dn1[c].init(h1); dn2[c].init(h2); }
        for (auto* r : {&dryL, &dryR, &preL, &preR}) r->assign(kRing, 0.f);
        gR.fill(1.f); cR.fill(1.f); bR.fill(0.f); fbR.fill(0.f); blR.fill(0.f);
        reset(); seenVer = -1; lutInterp = -1;
    }
    void reset() {
        for (int c = 0; c < 2; ++c) { up1[c].reset(); up2[c].reset(); dn1[c].reset(); dn2[c].reset(); ic1[c] = ic2[c] = 0; adPrev[c] = 0; dcX[c] = dcY[c] = 0; ex[c][0] = ex[c][1] = 0; }
        for (auto* r : {&dryL, &dryR, &preL, &preR}) std::fill(r->begin(), r->end(), 0.f);
        w = 0; phase = 0; m1 = m2 = 0; fresh = true; xf = 0.f; gPrev[0] = gPrev[1] = 1.f;
    }
    int latency() const { return kLatency; }
    void set(const Params& np) { p = np; }

    // Forme : écrite par le fil de message, lue sans attente par le fil audio (essai de verrou, copie de 400 octets).
    void pushShape(const Shape& s) {
        while (busy.exchange(true, std::memory_order_acquire)) {}
        box = s; ver.fetch_add(1, std::memory_order_relaxed);
        busy.store(false, std::memory_order_release);
    }

    // Début d'un bloc hôte : transport et positions des notes (retrig MIDI), relatives au début du bloc.
    void beginBlock(const Host& h, const int* notes, int nNotes) {
        nEv = std::min(nNotes, (int)ev.size()); for (int i = 0; i < nEv; ++i) ev[(size_t)i] = notes[i];
        evI = 0; pos = 0;
        const double bpm = h.hasBpm && h.bpm > 1 ? h.bpm : 120.0, Q = cycleQuarters(p.div, p.feel);
        inc = p.sync ? bpm / 60.0 / Q / sr : std::clamp((double)p.hz, 0.001, 200.0) / sr;
        if (p.sync && p.retrig == 0 && h.playing && h.hasPos) phase = h.ppq / Q;   // calé sur la position de l'hôte
        else phase -= std::floor(phase);
        // nouvelle forme ou nouveau tracé : retabuler, fondu de 20 ms depuis ce qu'on entendait
        if (seenVer != ver.load(std::memory_order_relaxed) || p.interp != lutInterp) {
            if (!busy.exchange(true, std::memory_order_acquire)) {
                local = box; seenVer = ver.load(std::memory_order_relaxed); busy.store(false, std::memory_order_release);
                for (int i = 0; i <= kLut; ++i) prev[(size_t)i] = cur[(size_t)i] + xf * (prev[(size_t)i] - cur[(size_t)i]);
                render(local, p.interp, cur.data()); lutInterp = p.interp;
                xf = fresh ? 0.f : 1.f;
            }
        }
    }

    // Traite n échantillons (inR nul : entrée mono). Les sorties peuvent être les entrées.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        const float fsr = (float)sr;
        const float aP = 1.f - std::exp(-1.f / (0.012f * fsr));                         // réglages : ~12 ms
        const float aM = 1.f - std::exp(-1.f / (std::max(0.25f, p.smoothMs * 0.5f) * 1e-3f * fsr));   // forme : 2 pôles
        const float aX = 1.f / (0.020f * fsr);
        const float tV = p.volOn ? p.volDepth : 0.f, tF = p.fltOn ? p.fltDepth : 0.f, tD = p.drvOn ? p.drvDepth : 0.f, tP = p.panOn ? 1.f : 0.f;
        const float tFreq = std::log(std::clamp(p.fltFreq, 20.f, 20000.f)), tRes = std::clamp(p.fltRes, 0.f, 1.f);
        const float tW[3] = {p.fltMode == 0 ? 1.f : 0.f, p.fltMode == 1 ? 1.f : 0.f, p.fltMode == 2 ? 1.f : 0.f};
        const float nyq = 0.45f * fsr, lOpenLP = std::log(std::min(20000.f, nyq)), lOpenHP = std::log(16.f);
        const float tMix = std::clamp(p.mix, 0.f, 1.f), tOut = std::pow(10.f, std::clamp(p.outDb, -24.f, 24.f) / 20.f);
        const float phOff = p.phase / 360.f;
        const int type = std::clamp(p.drvType, 0, 2);
        if (fresh) {   // premier bloc : tout part de sa cible (rien ne glisse au chargement)
            sV = tV; sF = tF; sD = tD; sP = tP; sFreq = tFreq; sRes = tRes; for (int k = 0; k < 3; ++k) sW[k] = tW[k];
            sDrive = p.drive; sColor = p.color; sPan = p.pan; sWid = p.width; sMix = tMix; sOut = tOut;
            const float v = lutAt((float)(phase + phOff - std::floor(phase + phOff))); m1 = m2 = v; fresh = false;
        }
        for (int i = 0; i < n; ++i, ++pos) {
            while (evI < nEv && ev[(size_t)evI] <= pos) { if (p.retrig == 1) phase = 0; ++evI; }
            double ph = phase + phOff; ph -= std::floor(ph);
            const float raw = lutAt((float)ph);
            phase += inc;
            if (p.retrig != 0 || !p.sync) phase -= std::floor(phase);
            m1 += aM * (raw - m1); m2 += aM * (m1 - m2);
            if (std::fabs(raw - m2) < 1e-7f && std::fabs(raw - m1) < 1e-7f) m1 = m2 = raw;
            if (xf > 0.f) xf = std::max(0.f, xf - aX);
            const float m = m2;
            sm(sV, tV, aP); sm(sF, tF, aP); sm(sD, tD, aP); sm(sP, tP, aP); sm(sFreq, tFreq, aP); sm(sRes, tRes, aP);
            for (int k = 0; k < 3; ++k) sm(sW[k], tW[k], aP);
            sm(sDrive, p.drive, aP); sm(sColor, p.color, aP); sm(sPan, p.pan, aP); sm(sWid, p.width, aP); sm(sMix, tMix, aP); sm(sOut, tOut, aP);

            const float x0 = inL[i], x1 = inR ? inR[i] : x0;
            float l = x0, r = x1;
            // --- filtre
            const float eF = sF * m;
            if (eF > 0.f || sF > 0.f) {
                const float lOpen = sW[0] * lOpenLP + sW[1] * std::min(sFreq + 2.08f, std::log(nyq)) + sW[2] * lOpenHP;
                const float fc = std::exp(lOpen + (sFreq - lOpen) * eF);
                const float g = std::tan((float)kPi * std::min(fc, nyq) / fsr), k = 2.f - 1.86f * sRes;
                const float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
                const float bl = blendOf(eF);
                float* ch[2] = {&l, &r};
                for (int c = 0; c < 2; ++c) {
                    const float x = *ch[c], v3 = x - ic2[c], v1 = a1 * ic1[c] + a2 * v3, v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                    ic1[c] = 2.f * v1 - ic1[c]; ic2[c] = 2.f * v2 - ic2[c];
                    const float y = sW[0] * v2 + sW[1] * k * v1 + sW[2] * (x - k * v1 - v2);
                    *ch[c] = x + bl * (y - x);
                }
            }
            // --- volume
            const float gV = 1.f - sV * m; l *= gV; r *= gV;
            // --- pan / largeur
            const float eP = sP * m;
            if (eP != 0.f) {
                const float wd = 1.f + sWid * eP, pn = std::clamp(sPan * eP, -1.f, 1.f);
                if (wd != 1.f) { const float M = 0.5f * (l + r), S = 0.5f * (l - r) * wd; l = M + S; r = M - S; }
                l *= pn > 0.f ? std::cos(pn * (float)kPi * 0.5f) : 1.f; r *= pn < 0.f ? std::cos(-pn * (float)kPi * 0.5f) : 1.f;
            }
            // --- distorsion lisse (×4) : drive aligné sur le signal sur-échantillonné, mélange aligné sur la sortie
            const float eD = sD * m;
            const double gNow = std::pow(10.0, (double)(sDrive * eD) / 20.0), bNow = sColor * 0.6;
            const size_t iw = (size_t)(w & kPm);
            gR[iw] = (float)gNow; bR[iw] = (float)bNow; cR[iw] = (float)compensation(type, gNow, bNow);
            fbR[iw] = (float)shp(type, bNow); blR[iw] = blendOf(eD);
            const size_t iu = (size_t)((w - kUp) & kPm), io = (size_t)((w - kLatency) & kPm);
            const float gU = gR[iu], bU = bR[iu], cU = cR[iu], fU = fbR[iu];
            preL[(size_t)(w & kRm)] = l; preR[(size_t)(w & kRm)] = r;
            dryL[(size_t)(w & kRm)] = x0; dryR[(size_t)(w & kRm)] = x1;
            const float dL = preL[(size_t)((w - kLatency) & kRm)], dR = preR[(size_t)((w - kLatency) & kRm)];
            const float wl = oversample(0, l, gU, bU, cU, fU, type), wr = oversample(1, r, gU, bU, cU, fU, type);
            const float bl = blR[io];
            float yl = dL + bl * (wl - dL), yr = dR + bl * (wr - dR);
            const float xl = dryL[(size_t)((w - kLatency) & kRm)], xr = dryR[(size_t)((w - kLatency) & kRm)];
            yl = (xl + sMix * (yl - xl)) * sOut; yr = (xr + sMix * (yr - xr)) * sOut;
            outL[i] = yl; outR[i] = yr;
            ++w;
            lastM = m; lastPh = (float)ph; lastEnv[0] = sV * m; lastEnv[1] = eF; lastEnv[2] = eD; lastEnv[3] = eP; lastDriveDb = sDrive * eD;
        }
    }
    // Lecture (interface)
    float phaseNow() const { return lastPh; }
    float modNow() const { return lastM; }
    float env(int k) const { return lastEnv[k]; }
    float driveDbNow() const { return lastDriveDb; }

private:
    static inline void sm(float& v, float t, float a) { v += a * (t - v); if (std::fabs(t - v) < 1e-7f) v = t; }
    static inline float blendOf(float e) { const float t = std::min(1.f, e * 5.f); return t * t * (3.f - 2.f * t); }
    inline float lutAt(float ph) const {
        const float x = ph * (float)kLut; const int i = std::min(kLut - 1, (int)x); const float t = x - (float)i;
        const float a = cur[(size_t)i] + t * (cur[(size_t)i + 1] - cur[(size_t)i]);
        if (xf <= 0.f) return a;
        const float b = prev[(size_t)i] + t * (prev[(size_t)i + 1] - prev[(size_t)i]);
        return a + xf * (b - a);
    }
    inline float oversample(int c, float x, float g, float b, float comp, float fb, int type) {
        float a0, a1, s[4]; up1[c].push(x, a0, a1); up2[c].push(a0, s[0], s[1]); up2[c].push(a1, s[2], s[3]);
        const float g0 = gPrev[c]; gPrev[c] = g;
        float v[4];
        for (int k = 0; k < 4; ++k) {
            double gk = g0 + (g - g0) * (float)(k + 1) * 0.25f; if (type == 2) gk = foldGain(gk);
            const double u = gk * s[k] + b, du = u - adPrev[c];
            const double y = std::fabs(du) > 1e-5 ? (prim(type, u) - prim(type, adPrev[c])) / du : shp(type, 0.5 * (u + adPrev[c]));
            adPrev[c] = u;
            const float out = (float)((y - fb) * comp);
            v[k] = ex[c][0]; ex[c][0] = ex[c][1]; ex[c][1] = out;   // retard de 2 échantillons ×4 (latence entière)
        }
        const float w0 = dn2[c].push(v[0], v[1]), w1 = dn2[c].push(v[2], v[3]);
        const float y = dn1[c].push(w0, w1);
        const float R = 1.f - (float)(2 * kPi * 5.0 / sr);   // bloque-continu 5 Hz (asymétrie)
        const float o = y - dcX[c] + R * dcY[c]; dcX[c] = y; dcY[c] = o;
        return o;
    }

    static constexpr int kPm = 127, kRing = 128, kRm = 127;
    Params p; double sr = 48000, phase = 0, inc = 0;
    HalfUp up1[2], up2[2]; HalfDown dn1[2], dn2[2];
    std::vector<float> dryL, dryR, preL, preR;
    std::array<float, kRing> gR{}, bR{}, cR{}, fbR{}, blR{};
    std::array<float, kLut + 1> cur{}, prev{};
    std::array<int, 128> ev{};
    Shape box, local; std::atomic<bool> busy{false}; std::atomic<int> ver{0}; int seenVer = -1, lutInterp = -1;
    int nEv = 0, evI = 0, pos = 0; unsigned w = 0;
    float m1 = 0, m2 = 0, xf = 0; bool fresh = true;
    float sV = 0, sF = 0, sD = 0, sP = 0, sFreq = 6, sRes = 0, sW[3] = {1, 0, 0}, sDrive = 0, sColor = 0, sPan = 0, sWid = 0, sMix = 1, sOut = 1;
    float ic1[2] = {}, ic2[2] = {}, dcX[2] = {}, dcY[2] = {}, ex[2][2] = {}, gPrev[2] = {1, 1};
    double adPrev[2] = {};
    float lastM = 0, lastPh = 0, lastEnv[4] = {}, lastDriveDb = 0;
};
}  // namespace gal
