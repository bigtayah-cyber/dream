// Moteur « Orbe » (TuneCore.h) : mesures objectives et comparaison chiffrée avec l'ancien moteur (atn::Engine classique).
// Cible : tune_core_test (EXCLUDE_FROM_ALL). Usage : tune_core_test [prise.wav]
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <new>
#include <random>
#include "../Source/Suite/OrbeAutotune/AutotuneDsp.h"

// ---- compteur d'allocations (vérifie : aucune allocation pendant process)
static std::atomic<long> gAllocs{0};
static bool gCount = false;
void* operator new(size_t n) { if (gCount) ++gAllocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("  ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)
using V = std::vector<float>;
static double SR = 44100.0;

// ---------------------------------------------------------------- signaux
// Enveloppe de formants connue (3 bosses) : sert à mesurer la stabilité des formants. Les bosses sont plus larges que l'écart
// entre harmoniques (156 Hz) : aucune méthode ne peut retrouver une enveloppe plus fine que l'échantillonnage par les harmoniques.
static double formantG(double f) {
    auto b = [&](double c, double w, double g) { return g * std::exp(-std::pow((f - c) / w, 2.0)); };
    return 0.04 + b(700, 300, 1.0) + b(1250, 350, 0.55) + b(2600, 450, 0.25);   // bosses plus larges que l'écart entre harmoniques : enveloppe observable
}
// Voix synthétique : f0(t) en MIDI (fonction), harmoniques pondérées par formantG (ou 1/k si flat).
template <class F> static V voice(F midiAt, double dur, bool formants = true, double amp = 0.25) {
    V x; double ph = 0; const size_t n = (size_t)(dur * SR);
    for (size_t i = 0; i < n; ++i) {
        const double t = (double)i / SR, f0 = 440.0 * std::pow(2.0, (midiAt(t) - 69.0) / 12.0);
        ph += 2 * M_PI * f0 / SR; double v = 0;
        for (int k = 1; k * f0 < 5000.0 && k <= 60; ++k) v += (formants ? formantG(k * f0) : 1.0 / k) * std::sin(k * ph);
        const double env = std::min(1.0, std::min((double)i, (double)(n - i)) / (0.02 * SR));
        x.push_back((float)(amp * v * env));
    }
    return x;
}
static V noise(double dur, double amp, unsigned seed, bool lowpass = false) {
    std::mt19937 r(seed); std::normal_distribution<float> g(0.f, 1.f); V x; float lp = 0;
    for (size_t i = 0; i < (size_t)(dur * SR); ++i) { float v = g(r); if (lowpass) { lp += 0.08f * (v - lp); v = lp * 3.f; } x.push_back((float)amp * v); }
    return x;
}

// ---------------------------------------------------------------- mesure de hauteur de référence (YIN plein débit, hors-ligne)
static V refPitch(const V& x, int hopN = 256) {
    const int Wn = 1024, tmax = (int)(SR / 60.0), tmin = (int)(SR / 1100.0); V out; std::vector<double> d((size_t)tmax + 2);
    for (size_t s = (size_t)(Wn + tmax); s < x.size(); s += (size_t)hopN) {
        const float* a = &x[s - (size_t)Wn];
        double e = 0; for (int j = 0; j < Wn; ++j) e += a[j] * a[j];
        if (e / Wn < 1e-6) { out.push_back(0.f); continue; }
        double cum = 0; d[0] = 1; float res = 0.f;
        for (int t = 1; t <= tmax; ++t) { double s2 = 0; for (int j = 0; j < Wn; ++j) { const double v = a[j] - a[j - t]; s2 += v * v; } cum += s2; d[(size_t)t] = cum > 0 ? s2 * t / cum : 1; }
        for (int t = tmin + 1; t < tmax; ++t)
            if (d[(size_t)t] < 0.15 && d[(size_t)t] <= d[(size_t)t - 1] && d[(size_t)t] < d[(size_t)t + 1]) {
                const double a0 = d[(size_t)t - 1], b0 = d[(size_t)t], c0 = d[(size_t)t + 1], den = a0 - 2 * b0 + c0;
                const double tf = t + (den > 1e-12 ? 0.5 * (a0 - c0) / den : 0.0);
                res = (float)(69.0 + 12.0 * std::log2(SR / tf / 440.0)); break;
            }
        out.push_back(res);
    }
    return out;   // trame k : fin de fenêtre à Wn + tmax + k*hop
}
static float median(V v) { if (v.empty()) return 0; std::sort(v.begin(), v.end()); return v[v.size() / 2]; }
static float pct(V v, float q) { if (v.empty()) return 0; std::sort(v.begin(), v.end()); return v[(size_t)std::min<double>(v.size() - 1, q * v.size())]; }
static size_t frameAt(double t, int hopN = 256) { const double s = t * SR - (1024 + (int)(SR / 60.0)); return s < 0 ? 0 : (size_t)(s / hopN); }

// ---------------------------------------------------------------- moteurs
enum Algo { OldQ, OldD, OrbeQ, OrbeD };
static const char* algoName(Algo a) { return a == OldQ ? "ancien Qualite" : a == OldD ? "ancien Direct " : a == OrbeQ ? "Orbe Qualite  " : "Orbe Direct   "; }
static atn::Params base(Algo a) { atn::Params p; p.amt = 1; p.tol = 0; p.speed = 0; p.dbl = 0; p.harm = 0; p.mix = 1; p.form = 0; p.vd = 0; p.trans = 0; p.key = 0; p.scale = 8;
    p.lowLat = a == OldD || a == OrbeD; p.orbe = a == OrbeQ || a == OrbeD; p.expr = 1.f; return p; }
static V runE(atn::Engine& e, const V& in) {
    V l = in, r = in; std::mt19937 rng(5);
    for (size_t p = 0; p < in.size();) { const size_t c = std::min<size_t>(in.size() - p, 1 + rng() % 700); e.process(l.data() + p, r.data() + p, (int)c); p += c; }
    return l;
}
static V runA(Algo a, const atn::Params& p, const V& in, int* lat = nullptr) {
    static atn::Engine e; e.prepare(SR); e.set(p); if (lat) *lat = e.latency(); return runE(e, in);
}
// Hauteur de sortie réalignée sur l'entrée (en trames de 256 échantillons).
static V outPitch(const V& y, int lat) { V z(y.begin() + std::min<size_t>(y.size(), (size_t)lat), y.end()); return refPitch(z); }

// Spectre (Blackman-Harris, N = 65536) ; renvoie la puissance par case.
static std::vector<double> spectrum(const V& y, size_t a, size_t N = 65536) {
    std::vector<double> re(N), im(N, 0.0);
    for (size_t i = 0; i < N; ++i) { const double w = 0.35875 - 0.48829 * std::cos(2 * M_PI * i / (N - 1)) + 0.14128 * std::cos(4 * M_PI * i / (N - 1)) - 0.01168 * std::cos(6 * M_PI * i / (N - 1)); re[i] = w * (a + i < y.size() ? y[a + i] : 0.f); }
    for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
    for (size_t len = 2; len <= N; len <<= 1) { const double ang = -2 * M_PI / (double)len;
        for (size_t i = 0; i < N; i += len) for (size_t k = 0; k < len / 2; ++k) { const double wr = std::cos(ang * k), wi = std::sin(ang * k); const size_t a0 = i + k, b0 = a0 + len / 2;
            const double tr = re[b0] * wr - im[b0] * wi, ti = re[b0] * wi + im[b0] * wr; re[b0] = re[a0] - tr; im[b0] = im[a0] - ti; re[a0] += tr; im[a0] += ti; } }
    std::vector<double> p(N / 2); for (size_t k = 0; k < N / 2; ++k) p[k] = re[k] * re[k] + im[k] * im[k]; return p;
}
// THD+N : énergie hors harmoniques de f0 (garde 30 Hz), en dB.
static double thdn(const V& y, size_t a, double f0, int nh) {
    auto p = spectrum(y, a); const double bw = SR / 65536.0; double all = 0, sig = 0;
    for (size_t k = 1; k < p.size(); ++k) { const double f = k * bw; if (f > 6000) break; all += p[k]; const int h = (int)std::lround(f / f0); if (h >= 1 && h <= nh && std::fabs(f - h * f0) < 30.0) sig += p[k]; }
    return 10 * std::log10((all - sig) / all + 1e-30);
}
// Erreur d'enveloppe (dB RMS, moyenne retirée) : amplitudes des harmoniques de f0 entre 250 et 3500 Hz contre formantG.
static double envErr(const V& y, size_t a, double f0) {
    auto p = spectrum(y, a); const double bw = SR / 65536.0; std::vector<double> d;
    for (int k = 1; k * f0 < 3500; ++k) { const double f = k * f0; if (f < 250) continue; const int c = (int)std::lround(f / bw); double m = 0; for (int j = c - 8; j <= c + 8; ++j) m = std::max(m, p[(size_t)j]);
        d.push_back(10 * std::log10(m + 1e-30) - 20 * std::log10(formantG(f))); }
    double mu = 0; for (double v : d) mu += v; mu /= d.size(); double s = 0; for (double v : d) s += (v - mu) * (v - mu); return std::sqrt(s / d.size());
}

struct Wav { V x; };
static bool readWav(const char* path, Wav& w) {
    FILE* f = std::fopen(path, "rb"); if (!f) return false;
    std::vector<uint8_t> b; uint8_t t[65536]; size_t r; while ((r = std::fread(t, 1, sizeof t, f)) > 0) b.insert(b.end(), t, t + r); std::fclose(f);
    size_t pos = 12; int nch = 2; uint16_t bits = 16;
    while (pos + 8 <= b.size()) { uint32_t sz; std::memcpy(&sz, &b[pos + 4], 4);
        if (!std::memcmp(&b[pos], "fmt ", 4)) { uint16_t c; std::memcpy(&c, &b[pos + 10], 2); nch = c; std::memcpy(&bits, &b[pos + 22], 2); }
        else if (!std::memcmp(&b[pos], "data", 4) && bits == 16) { const size_t n = std::min<size_t>(sz, b.size() - pos - 8) / (2 * (size_t)nch);
            for (size_t i = 0; i < n; ++i) { int16_t s, s2 = 0; std::memcpy(&s, &b[pos + 8 + i * 2 * nch], 2); if (nch > 1) std::memcpy(&s2, &b[pos + 10 + i * 2 * nch], 2); w.x.push_back(nch > 1 ? (s + s2) / 65536.f : s / 32768.f); } return true; }
        pos += 8 + sz + (sz & 1); }
    return false;
}

static const Algo kAll[4] = {OldQ, OldD, OrbeQ, OrbeD};

int main(int argc, char** argv) {
    std::printf("=== Orbe : moteur TuneCore, mesures objectives (44,1 kHz sauf mention) ===\n");
    // ---------------------------------------------------- 1. latence déclarée = mesurée (impulsion, et voix corrigée par corrélation)
    std::printf("\n[1] latence\n");
    for (Algo a : kAll) {
        auto p = base(a); p.amt = 0; int lat; V in(30000, 0.f); in[5000] = 1.f; const V y = runA(a, p, in, &lat);
        int pk = 0; for (size_t i = 0; i < y.size(); ++i) if (std::fabs(y[i]) > std::fabs(y[(size_t)pk])) pk = (int)i;
        // voix +30 ct corrigée, modulée en amplitude par un bruit lent : retard de l'enveloppe (sans ambiguïté de période)
        auto q = base(a); q.speed = 0; V v = voice([](double) { return 57.3; }, 3.0);
        { std::mt19937 rg(11); std::normal_distribution<float> g; float m1 = 0, m2 = 0; for (auto& s : v) { m1 += 0.02f * (g(rg) - m1); m2 += 0.02f * (m1 - m2); s *= std::max(0.05f, 1.f + 4.f * m2); } }
        auto env = [](const V& x) { V e(x.size()); float s = 0; for (size_t i = 0; i < x.size(); ++i) { s += 0.05f * (std::fabs(x[i]) - s); e[i] = s; } return e; };
        auto lagOf = [&](const V& w) { const V ev = env(v), ew = env(w); double best = -1; int bl = 0;
            for (int lag = std::max(0, lat - 600); lag <= lat + 600; ++lag) { double xy = 0, ee = 1e-12, ff = 1e-12;
                for (size_t j = (size_t)(0.4 * SR); j + lag < ew.size() && j < (size_t)(2.8 * SR); j += 4) { const double a0 = ev[j], b0 = ew[j + lag]; xy += a0 * b0; ee += a0 * a0; ff += b0 * b0; }
                const double c = xy / std::sqrt(ee * ff); if (c > best) { best = c; bl = lag; } }
            return bl; };
        auto q0 = q; q0.amt = 0; const int bd = lagOf(runA(a, q0, v)), bl = lagOf(runA(a, q, v)) - bd + lat;
        std::printf("  %s : declaree %4d (%.1f ms), impulsion %4d, voix corrigee %d (mesure par enveloppe, calee sur le temoin non corrige)\n", algoName(a), lat, 1000.0 * lat / SR, pk - 5000, bl);
        CHECK(pk - 5000 == lat, "%s latence impulsion %d != %d", algoName(a), pk - 5000, lat);
        // le lecteur de raccords oscille par construction entre L - T0/2 et L + T0/2 (moyenne L) : tolérance 0,3 période
        if (a == OrbeQ || a == OrbeD) CHECK(std::abs(bl - lat) <= 60, "%s retard de la voix corrigee %d loin de %d", algoName(a), bl, lat);
    }
    // ---------------------------------------------------- 2. erreur de hauteur après correction (+30 ct ramené sur la note)
    std::printf("\n[2] erreur de hauteur apres correction (median |ecart| en cents, regime etabli 0,6-1,8 s)\n");
    for (float sp : {0.f, 20.f}) for (double f0m : {45.0, 55.0, 64.0, 74.0}) {
        std::printf("  vitesse %2.0f ms, note %2.0f + 30 ct :", sp, f0m);
        for (Algo a : kAll) {
            auto p = base(a); p.speed = sp; int lat; const V y = runA(a, p, voice([&](double) { return f0m + 0.3; }, 2.0), &lat);
            const V tr = outPitch(y, lat); V d; for (size_t k = frameAt(0.6); k < std::min(tr.size(), frameAt(1.8)); ++k) if (tr[k] > 0) d.push_back(std::fabs(tr[k] - (float)f0m) * 100.f);
            const float m = d.empty() ? 999.f : median(d); std::printf(" %s %5.1f |", algoName(a) + 0, m);
            if (a == OrbeQ || a == OrbeD) CHECK(m <= 3.f, "%s note %.0f vitesse %.0f : %.1f ct", algoName(a), f0m, sp, m);
        }
        std::printf("\n");
    }
    // ---------------------------------------------------- 3. vibrato : conservé en réglage naturel, aplati en robot
    std::printf("\n[3] vibrato 5,5 Hz +-40 ct, centre +25 ct (A3) : centre de sortie et profondeur sortie/entree\n");
    {
        auto f = [](double t) { return 57.25 + 0.40 * std::sin(2 * M_PI * 5.5 * t) * std::min(1.0, t / 0.2); };
        const V in = voice(f, 3.0); const V ti = refPitch(in);
        auto depth = [&](const V& tr, float& ctr) { V d0, d; for (size_t k = frameAt(1.0); k < std::min(tr.size(), frameAt(2.8)); ++k) if (tr[k] > 0) d0.push_back(tr[k]);
            const float md = median(d0); for (float v : d0) if (std::fabs(v - md) < 1.5f) d.push_back(v);   // trames aberrantes de la mesure écartées
            double mu = 0; for (float v : d) mu += v; mu /= std::max<size_t>(1, d.size()); double s = 0; for (float v : d) s += (v - mu) * (v - mu); ctr = (float)((mu - 57.0) * 100.0); return (float)(std::sqrt(s / std::max<size_t>(1, d.size())) * std::sqrt(2.0) * 100.0); };
        float ci; const float di = depth(ti, ci);
        std::printf("  entree : centre %+.1f ct, profondeur %.1f ct\n", ci, di);
        for (float sp : {0.f, 120.f}) for (Algo a : kAll) {
            auto p = base(a); p.speed = sp; int lat; const V y = runA(a, p, in, &lat); float co; const float dout = depth(outPitch(y, lat), co);
            std::printf("  vitesse %3.0f ms %s : centre %+6.1f ct, profondeur %5.1f ct (x%.2f)\n", sp, algoName(a), co, dout, dout / di);
            if (a == OrbeQ || a == OrbeD) {
                if (sp > 100) { CHECK(dout / di > 0.85f && dout / di < 1.15f, "%s naturel : vibrato x%.2f", algoName(a), dout / di); CHECK(std::fabs(co) < 6.f, "%s naturel : centre %+.1f", algoName(a), co); }
                else CHECK(dout / di < 0.3f, "%s robot : vibrato non aplati x%.2f", algoName(a), dout / di);
            }
        }
    }
    // ---------------------------------------------------- 4. glissando volontaire (57 -> 60 en 400 ms) en réglage naturel
    std::printf("\n[4] glissando 3 dt en 400 ms : ecart median sortie/entree pendant le glissement (cents)\n");
    {
        auto f = [](double t) { return t < 0.8 ? 57.0 : t < 1.2 ? 57.0 + 3.0 * (t - 0.8) / 0.4 : 60.0; };
        const V in = voice(f, 2.0); const V ti = refPitch(in);
        for (float sp : {0.f, 120.f}) for (Algo a : kAll) {
            auto p = base(a); p.speed = sp; int lat; const V yo = runA(a, p, in, &lat); const V to = outPitch(yo, lat); V d;
            for (size_t k = frameAt(0.9); k < std::min({ti.size(), to.size(), frameAt(1.15)}); ++k) if (ti[k] > 0 && to[k] > 0) d.push_back(std::fabs(ti[k] - to[k]) * 100.f);
            std::printf("  vitesse %3.0f ms %s : %.1f ct\n", sp, algoName(a), median(d));
            if ((a == OrbeQ || a == OrbeD) && sp > 100) CHECK(median(d) < 15.f, "%s glissando aplati : %.1f ct", algoName(a), median(d));
        }
    }
    // ---------------------------------------------------- 5. formants : enveloppe avant/après transposition (+-4 dt)
    std::printf("\n[5] stabilite des formants : erreur d'enveloppe (dB RMS, harmoniques 250-3500 Hz)\n");
    {
        const V in = voice([](double) { return 51.0; }, 3.0);   // D#3 ~ 156 Hz
        std::printf("  entree : %.2f dB\n", envErr(in, (size_t)(1.2 * SR), 440.0 * std::pow(2.0, (51.0 - 69.0) / 12.0)));
        for (int tr : {4, -4}) for (int fp = 1; fp >= 0; --fp) for (Algo a : kAll) {
            if (!fp && (a == OldD)) continue;
            auto p = base(a); p.amt = 0; p.trans = (float)tr; p.formPres = fp; int lat; const V y = runA(a, p, in, &lat);
            const double e = envErr(y, (size_t)(1.2 * SR) + lat, 440.0 * std::pow(2.0, (51.0 + tr - 69.0) / 12.0));
            std::printf("  %+d dt, formants %s, %s : %5.2f dB\n", tr, fp ? "preserves" : "libres   ", algoName(a), e);
            if (fp && (a == OrbeQ || a == OrbeD)) CHECK(e < 3.0, "%s formants %+d dt : %.2f dB", algoName(a), tr, e);
        }
    }
    // ---------------------------------------------------- 6. THD+N (voix corrigée +30 ct -> note)
    std::printf("\n[6] THD+N d'une voix corrigee (dB, plus bas = mieux)\n");
    {
        struct Cs { const char* n; double m; int nh; bool fl; };
        const Cs cs[4] = {{"sinus 440 Hz", 69.0, 1, false}, {"voix 110 Hz", 45.0, 8, false}, {"voix 196 Hz", 55.0, 8, false}, {"voix 330 Hz", 64.0, 8, false}};
        for (const auto& c : cs) { std::printf("  %-12s :", c.n);
            for (Algo a : kAll) {
                V in; { double ph = 0; const double f = 440.0 * std::pow(2.0, (c.m + 0.3 - 69.0) / 12.0); const size_t n = (size_t)(4.0 * SR);
                    for (size_t i = 0; i < n; ++i) { ph += 2 * M_PI * f / SR; double v = 0; for (int k = 1; k <= c.nh; ++k) v += std::sin(k * ph) / (c.nh > 1 ? k : 1); in.push_back((float)(0.3 * v * std::min(1.0, std::min((double)i, (double)(n - i)) / (0.02 * SR)))); } }
                auto p = base(a); int lat; const V y = runA(a, p, in, &lat);
                const double db = thdn(y, (size_t)(2.0 * SR), 440.0 * std::pow(2.0, (c.m - 69.0) / 12.0), c.nh);
                std::printf(" %s %6.1f |", algoName(a), db);
            }
            std::printf("\n"); }
    }
    // ---------------------------------------------------- 7. voisé / non voisé : consonnes et souffle rendus intacts
    std::printf("\n[7] segments non voises (bruit 's', souffle) : ecart sortie / entree retardee (dB, plus bas = intact)\n");
    {
        V in = voice([](double) { return 57.4; }, 0.6); const size_t s1 = in.size(); V nz = noise(0.3, 0.08, 3); in.insert(in.end(), nz.begin(), nz.end()); const size_t e1 = in.size();
        V v2 = voice([](double) { return 60.4; }, 0.6); in.insert(in.end(), v2.begin(), v2.end()); const size_t s2 = in.size(); V br = noise(0.4, 0.01, 4, true); in.insert(in.end(), br.begin(), br.end()); const size_t e2 = in.size();
        V tail = voice([](double) { return 62.4; }, 0.4); in.insert(in.end(), tail.begin(), tail.end());
        for (Algo a : kAll) {
            auto p = base(a); p.speed = 0; int lat; const V y = runA(a, p, in, &lat);
            auto err = [&](size_t a0, size_t b0) { const size_t g = (size_t)(0.03 * SR); double e = 0, s = 1e-20; for (size_t i = a0 + g; i < b0 - g; ++i) { const double d = y[i + lat] - in[i]; e += d * d; s += (double)in[i] * in[i]; } return 10 * std::log10(e / s + 1e-30); };
            const double es = err(s1, e1), eb = err(s2, e2);
            std::printf("  %s : 's' %7.1f dB, souffle %7.1f dB\n", algoName(a), es, eb);
            if (a == OrbeQ || a == OrbeD) { CHECK(es < -60.0, "%s 's' modifie : %.1f dB", algoName(a), es); CHECK(eb < -60.0, "%s souffle modifie : %.1f dB", algoName(a), eb); }
        }
    }
    // ---------------------------------------------------- 8. prise réelle
    std::printf("\n[8] prise reelle\n");
    {
        Wav w; const char* path = argc > 1 ? argv[1] : "/tmp/claude-0/-home-claude/12ae9892-af71-5ca8-a97b-93f9fd958b0b/scratchpad/prise.wav";
        if (readWav(path, w)) {
            const V ti = refPitch(w.x);
            int nv = 0; for (float v : ti) nv += v > 0; std::printf("  %zu trames, %d voisees (reference YIN plein debit)\n", ti.size(), nv);
            // détection : écarts grossiers (> 50 ct, octaves) de la hauteur interne de chaque moteur contre la référence
            {
                tc::TuneCore c; c.prepare(SR, 220, 706); c.setQuality(true); tc::Params q; c.setParams(q);
                V l = w.x, r = w.x; V cp; const int hp = c.hopSize();
                for (size_t i = 0; i + (size_t)hp <= l.size(); i += (size_t)hp) { c.process(l.data() + i, r.data() + i, hp); cp.push_back(c.state().voiced ? c.state().pitch : 0.f); }
                atn::Tracker t; t.init(SR, 128); V op; for (size_t i = 0; i < w.x.size(); ++i) { t.det.push(w.x[i]); if ((i + 1) % 128 == 0) op.push_back(t.step() ? t.midi : 0.f); }
                // alignement : trame de référence k (fin à 1024+735+256k, centre -512) ; Orbe Q : centre = fin - 706 ; ancien : centre = fin - ~512
                auto cmp = [&](const V& tr, double centreLag, int& both, int& gross, int& oct, int& miss) { both = gross = oct = miss = 0;
                    for (size_t k = 0; k < ti.size(); ++k) { const double centre = 1024 + (int)(SR / 60.0) + 256.0 * k - 512; const long j = std::lround((centre + centreLag) / 128.0) - 1; if (j < 0 || j >= (long)tr.size()) continue;
                        if (ti[k] > 0 && tr[(size_t)j] <= 0) ++miss;
                        if (ti[k] > 0 && tr[(size_t)j] > 0) { ++both; const float d = std::fabs(tr[(size_t)j] - ti[k]); gross += d > 0.5f; oct += std::fabs(d - 12.f) < 1.f || std::fabs(d - 19.f) < 1.f; } } };
                int b, g, o, m; cmp(cp, 706, b, g, o, m); std::printf("  detection Orbe Q : %d trames communes, %.2f %% > 50 ct, %d erreurs d'octave, %d voisees manquees\n", b, 100.0 * g / std::max(1, b), o, m);
                cmp(op, 512 + 256, b, g, o, m); std::printf("  detection ancien : %d trames communes, %.2f %% > 50 ct, %d erreurs d'octave, %d voisees manquees\n", b, 100.0 * g / std::max(1, b), o, m);
            }
            for (int mode = 0; mode < 2; ++mode) for (Algo a : kAll) {
                auto p = base(a); p.speed = mode == 0 ? 0.f : 120.f; int lat; const V y = runA(a, p, w.x, &lat); const V to = outPitch(y, lat);
                V dev, mv; int on10 = 0, on25 = 0, n2 = 0;
                for (size_t k = 0; k < std::min(ti.size(), to.size()); ++k) if (ti[k] > 0 && to[k] > 0) { ++n2; const float g = std::fabs(to[k] - std::round(to[k])) * 100.f; on10 += g <= 10.f; on25 += g <= 25.f; mv.push_back(std::fabs(to[k] - ti[k]) * 100.f); }
                bool fin = true; float pk = 0; for (float v : y) { fin &= std::isfinite(v); pk = std::max(pk, std::fabs(v)); }
                std::printf("  %s %s : %4.1f %% a <=10 ct de la grille, %4.1f %% a <=25 ct, deplacement median %.1f ct, pic %.2f\n", mode ? "naturel 120 ms" : "robot 0 ms    ", algoName(a), 100.0 * on10 / std::max(1, n2), 100.0 * on25 / std::max(1, n2), median(mv), pk);
                CHECK(fin, "NaN prise");
            }
        } else std::printf("  prise.wav absente : saute\n");
    }
    // ---------------------------------------------------- 9. taux d'échantillonnage, robustesse, reset, allocations
    std::printf("\n[9] taux d'echantillonnage, robustesse\n");
    for (double sr : {44100.0, 48000.0, 88200.0, 96000.0}) {
        SR = sr;
        for (Algo a : {OrbeQ, OrbeD}) {
            auto p = base(a); int lat; const V y = runA(a, p, voice([](double) { return 57.3; }, 2.0), &lat);
            const V tr = outPitch(y, lat); V d; for (size_t k = frameAt(0.6); k < std::min(tr.size(), frameAt(1.8)); ++k) if (tr[k] > 0) d.push_back(std::fabs(tr[k] - 57.f) * 100.f);
            std::printf("  %6.0f Hz %s : latence %d (%.2f ms), erreur %.1f ct\n", sr, algoName(a), lat, 1000.0 * lat / sr, median(d));
            CHECK(median(d) <= 3.f, "%.0f Hz %s : %.1f ct", sr, algoName(a), median(d));
            if (a == OrbeD) CHECK(lat <= 256, "latence Direct %d", lat);
        }
    }
    SR = 44100.0;
    {
        // entrées hostiles : NaN, inf, plein niveau, DC, silence ; puis reset -> même sortie qu'un moteur neuf
        tc::TuneCore c; c.prepare(SR, 220, 706); tc::Params q; q.mask = 0xFFF; q.speedMs = 0; c.setParams(q);
        V x = noise(1.0, 1.0, 9); for (size_t i = 0; i < x.size(); i += 997) x[i] = (i / 997) % 3 == 0 ? NAN : (i / 997) % 3 == 1 ? INFINITY : 30.f;
        V d(20000, 0.5f); x.insert(x.end(), d.begin(), d.end());
        bool ok = true; for (int qm = 0; qm < 2; ++qm) { c.setQuality(qm); V l = x, r = x; c.process(l.data(), r.data(), (int)l.size()); for (size_t i = 0; i < l.size(); ++i) ok &= std::isfinite(l[i]) && std::isfinite(r[i]) && std::fabs(l[i]) <= 8.f; }
        CHECK(ok, "sortie non finie ou hors borne sur entree hostile");
        const V v = voice([](double) { return 60.3; }, 1.0);
        c.reset(); V a1 = v, b1 = v; c.process(a1.data(), b1.data(), (int)a1.size());
        tc::TuneCore c2; c2.prepare(SR, 220, 706); c2.setQuality(true); c2.setParams(q); V a2 = v, b2 = v; c2.process(a2.data(), b2.data(), (int)a2.size());
        double e = 0; for (size_t i = 0; i < a1.size(); ++i) e = std::max(e, (double)std::fabs(a1[i] - a2[i]));
        std::printf("  entree hostile : %s ; reset : ecart max avec un moteur neuf %.2e\n", ok ? "sortie finie et bornee" : "ECHEC", e);
        CHECK(e < 1e-6, "reset incomplet : %g", e);
        // allocations pendant le traitement (moteur complet, mode Orbe, éditions et capture actives)
        static atn::Engine eng; auto p = base(OrbeQ); eng.prepare(SR); eng.set(p);
        static ned::EditList el; std::vector<ned::Note> nts(1); nts[0].start = 0; nts[0].end = 1e9; nts[0].center = 60.3f; nts[0].shift = -30.f; nts[0].edited = 1; el.assign(nts);
        static ned::Capture cap; cap.armed = 1;
        V l = v, r = v; gAllocs = 0; gCount = true;
        for (size_t i = 0; i + 512 <= l.size(); i += 512) { eng.setTimeline((double)i, true, &el, &cap); eng.process(l.data() + i, r.data() + i, 512); if (i == 20480) { p.lowLat = true; eng.set(p); } }
        gCount = false;
        std::printf("  allocations pendant process : %ld\n", gAllocs.load());
        CHECK(gAllocs.load() == 0, "%ld allocations en temps reel", gAllocs.load());
    }
    // ---------------------------------------------------- 10. édition note par note : note forcée par la timeline
    std::printf("\n[10] edition note par note\n");
    {
        // capture d'un passage (deux notes), découpage, la 2e note déplacée de +200 ct, relecture
        const V in = [] { V a = voice([](double t) { return t < 0.8 ? 57.1 : 62.2; }, 1.6); return a; }();
        static atn::Engine e; auto p = base(OrbeQ); p.amt = 0; e.prepare(SR); e.set(p);
        static ned::Capture cap; cap.hop = 128; cap.armed = 1;
        V l = in, r = in; for (size_t i = 0; i + 256 <= l.size(); i += 256) { e.setTimeline((double)i, true, nullptr, &cap); e.process(l.data() + i, r.data() + i, 256); }
        std::vector<ned::Frame> fr(cap.f.begin(), cap.f.begin() + cap.n.load());
        auto notes = ned::segment(fr.data(), (int)fr.size(), 128, SR);
        std::printf("  %zu trames capturees, %zu notes :", fr.size(), notes.size()); for (auto& q : notes) std::printf(" [%.2f-%.2f s, %.2f]", q.start / SR, q.end / SR, q.center); std::printf("\n");
        CHECK(notes.size() == 2, "decoupage : %zu notes au lieu de 2", notes.size());
        if (notes.size() == 2) {
            notes[1].shift = std::round((64.f - notes[1].center) * 100.f); notes[1].edited = 1; notes[1].speedMs = 0.f; notes[1].expr = 0.f;
            static ned::EditList el; el.assign(notes);
            e.prepare(SR); e.set(p); int lat = e.latency(); l = in; r = in;
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { e.setTimeline((double)i, true, &el, nullptr); e.process(l.data() + i, r.data() + i, 256); }
            const V to = outPitch(l, lat); V a, b; for (size_t k = frameAt(0.3); k < frameAt(0.7); ++k) if (to[k] > 0) a.push_back(to[k]); for (size_t k = frameAt(1.0); k < std::min(to.size(), frameAt(1.45)); ++k) if (to[k] > 0) b.push_back(to[k]);
            std::printf("  relecture : note 1 (non editee) %.2f, note 2 (editee -> 64) %.2f\n", median(a), median(b));
            CHECK(std::fabs(median(a) - 57.1f) < 0.05f, "note non editee deplacee : %.2f", median(a));
            CHECK(std::fabs(median(b) - 64.f) < 0.05f, "note editee : %.2f au lieu de 64", median(b));
            auto pk = ned::packNotes(notes); std::vector<ned::Note> back; ned::unpackNotes(pk.data(), pk.size(), back);
            auto pc = ned::packCurve(fr, 128); std::vector<ned::Frame> fb; ned::unpackCurve(pc.data(), pc.size(), fb);
            CHECK(back.size() == 2 && back[1].shift == notes[1].shift && back[1].edited == 1, "serialisation des notes");
            CHECK(fb.size() == fr.size() && std::fabs(fb.back().midi - fr.back().midi) < 0.006f && fb.back().pos == fr.back().pos, "serialisation de la courbe");
            std::printf("  sauvegarde : %zu octets (notes) + %zu octets (courbe)\n", pk.size(), pc.size());
        }
        // écriture des notes : dièses / bémols selon l'armure
        CHECK(!ned::useFlats(7, 0, true) && ned::useFlats(5, 0, false) && ned::useFlats(2, 1, false) && !ned::useFlats(4, 1, true), "armure : choix dieses / bemols");
        std::printf("  F majeur -> %s, G majeur -> %s, D mineur -> %s, E mineur -> %s\n", ned::noteName(10, ned::useFlats(5, 0, false)), ned::noteName(6, ned::useFlats(7, 0, true)), ned::noteName(10, ned::useFlats(2, 1, false)), ned::noteName(6, ned::useFlats(4, 1, true)));
    }
    // ---------------------------------------------------- 11. B1 : pas de fantôme d'harmonie / doublage à la reprise d'une phrase
    std::printf("\n[11] B1 : fantome d'harmonie / doublage a la reprise (dB sous la voix, 80 ms apres la reprise)\n");
    {
        auto build = [](bool breath) { V a = voice([](double) { return 57.0; }, 0.8, false); V b = breath ? noise(0.4, 0.05, 7, true) : V((size_t)(0.4 * SR), 0.f);
            a.insert(a.end(), b.begin(), b.end()); a.insert(a.end(), (size_t)(0.6 * SR), 0.f); V c = voice([](double) { return 57.0; }, 0.8, false); a.insert(a.end(), c.begin(), c.end()); return a; };
        const V x1 = build(true), x0 = build(false); const size_t rs = (size_t)(1.8 * SR);
        for (Algo a : kAll) for (int kind = 0; kind < 2; ++kind) {
            auto p = base(a); p.amt = 0; if (kind == 0) { p.harm = 4; p.hv = 1.f; } else p.dbl = 0.5f; int lat;
            const V y1 = runA(a, p, x1, &lat), y0 = runA(a, p, x0, &lat);
            double e = 1e-30, s0 = 1e-30; for (size_t i = rs + lat; i < rs + lat + (size_t)(0.08 * SR); ++i) { const double d = y1[i] - y0[i]; e += d * d; s0 += (double)y0[i] * y0[i]; }
            const double db = 10 * std::log10(e / s0);
            std::printf("  %s %s : %7.1f dB\n", algoName(a), kind ? "doublage 50 %" : "harmonie oct.", db);
            CHECK(db < -60.0, "%s fantome %s : %.1f dB", algoName(a), kind ? "doublage" : "harmonie", db);
        }
    }
    std::printf(fails ? "\n%d ECHEC(S)\n" : "\ntout est bon\n", fails);
    return fails ? 1 : 0;
}
