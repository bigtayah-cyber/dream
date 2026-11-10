// Tests objectifs des plugins à un potard (hors JUCE) : les mesures de l'audit 3 (docs/audit2/03-un-potard.md) devenues des seuils.
// g++ -O2 -std=c++17 -I Source tests/onek_test.cpp -o /tmp/onek_test && /tmp/onek_test [prise.wav]
// Options : --cpu (mesure du coût par instance), --rapide (saute 96 kHz et les blocs extrêmes).
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <new>
#include <string>
#include "../Source/OneKnob/OneKnob.h"
#include "../Source/Kit/BypassFadeTest.h"
using namespace onek;
static int fails = 0;
static bool gCountAlloc = false;
static long gAllocs = 0;
void* operator new(std::size_t n) { if (gCountAlloc) ++gAllocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { if (gCountAlloc) ++gAllocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("  ECHEC: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
using Vec = std::vector<float>;
constexpr double kPiD = 3.14159265358979323846;
static const char* nm(int m) { return voicings()[(size_t)m].name; }

static bool readWav(const char* path, Vec& mono, double& sr) {
    std::ifstream f(path, std::ios::binary); if (!f) return false;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    if (b.size() < 44 || std::memcmp(&b[0], "RIFF", 4)) return false;
    int ch = 1, bits = 16, fmt = 1; size_t p = 12, dpos = 0, dlen = 0;
    while (p + 8 <= b.size()) {
        uint32_t len; std::memcpy(&len, &b[p + 4], 4);
        if (!std::memcmp(&b[p], "fmt ", 4)) { uint16_t a, c, bb; uint32_t r; std::memcpy(&a, &b[p + 8], 2); std::memcpy(&c, &b[p + 10], 2); std::memcpy(&r, &b[p + 12], 4); std::memcpy(&bb, &b[p + 22], 2); fmt = a; ch = c; sr = r; bits = bb; }
        if (!std::memcmp(&b[p], "data", 4)) { dpos = p + 8; dlen = std::min<size_t>(len, b.size() - dpos); break; }
        p += 8 + len + (len & 1);
    }
    const int bps = bits / 8; const size_t frames = dlen / (size_t)(bps * ch);
    mono.assign(frames, 0.f);
    for (size_t i = 0; i < frames; ++i) {
        float acc = 0;
        for (int c = 0; c < ch; ++c) {
            const uint8_t* q = &b[dpos + (i * (size_t)ch + (size_t)c) * (size_t)bps]; float v;
            if (fmt == 3) { float t; std::memcpy(&t, q, 4); v = t; }
            else if (bits == 16) { int16_t t; std::memcpy(&t, q, 2); v = t / 32768.f; }
            else if (bits == 24) { int32_t t = (q[0] << 8) | (q[1] << 16) | (q[2] << 24); v = (float)(t >> 8) / 8388608.f; }
            else { int32_t t; std::memcpy(&t, q, 4); v = (float)t / 2147483648.f; }
            acc += v;
        }
        mono[i] = acc / (float)ch;
    }
    return true;
}

// ---------------------------------------------------------------- signaux
static uint32_t gSeed = 12345;
static float urnd() { gSeed = gSeed * 1664525u + 1013904223u; return (float)(gSeed >> 8) / 8388608.f - 1.f; }
// voix de synthèse : 10 harmoniques à enveloppe de formants, syllabes de 180 ms, vibrato, sifflantes brèves ; RMS voisé = levelDb
static Vec voice(double sr, double secs, double levelDb, double f0 = 150.0, bool syll = true) {
    Vec x((size_t)(secs * sr)); double ph = 0, s2 = 0; size_t nv = 0;
    float hp = 0, lpn = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        const double t = (double)i / sr, f = f0 * (1.0 + 0.012 * std::sin(2 * kPiD * 5.5 * t) + 0.04 * std::sin(2 * kPiD * 0.3 * t));
        ph += f / sr; ph -= std::floor(ph);
        double v = 0; for (int h = 1; h <= 10; ++h) { const double fh = h * f; const double g = 1.0 / (1.0 + std::pow((fh - 600) / 400, 2)) + 0.5 / (1.0 + std::pow((fh - 1800) / 500, 2)) + 0.15; v += g * std::sin(2 * kPiD * h * ph) / std::sqrt((double)h); }
        const double ts = std::fmod(t, 0.24);
        const double env = !syll ? 1.0 : ts < 0.18 ? std::sin(kPiD * ts / 0.18) : 0.0;
        const float n = urnd(); hp = 0.6f * (hp + n - lpn); lpn = n;   // bruit aigu (sifflante)
        const double sib = syll && std::fmod(t, 0.96) > 0.90 ? 0.6 : 0.0;
        x[i] = (float)(v * env + sib * hp);
        if (env > 0.1) { s2 += (double)x[i] * x[i]; ++nv; }
    }
    const float k = (float)(std::pow(10.0, levelDb / 20.0) / std::sqrt(s2 / (double)std::max<size_t>(1, nv)));
    for (auto& v : x) v *= k;
    return x;
}
static Vec sine(double sr, double secs, double f, float a) { Vec x((size_t)(secs * sr)); for (size_t i = 0; i < x.size(); ++i) x[i] = a * (float)std::sin(2 * kPiD * f * (double)i / sr); return x; }

struct Out { Vec L, R; int lat = 0; double gr = 0; };
// Traite x (mono, ou stéréo si xr) par blocs ; perBlock(chain, indexBloc) appelé avant chaque bloc.
template <class F>
static Out run(int model, float k, const Vec& x, double sr, int block, const Extras& e, F&& perBlock, const Vec* xr = nullptr, bool classicMode = false, double grFrom = -1) {
    Chain c; c.select(model); c.setClassic(classicMode); c.prepare(sr, std::min(block, 2048)); c.setExtras(e);
    Out o; o.L.assign(x.size(), 0.f); o.R.assign(x.size(), 0.f); o.lat = c.latency();
    double acc = 0; int nb = 0;
    for (size_t p = 0, bi = 0; p < x.size(); p += (size_t)block, ++bi) {
        const int n = (int)std::min<size_t>((size_t)block, x.size() - p);
        c.setKnob(k, voicings()[(size_t)model]);
        perBlock(c, (int)bi);
        c.process(&x[p], xr ? &(*xr)[p] : nullptr, &o.L[p], &o.R[p], n);
        if (grFrom >= 0 && (double)p > grFrom * sr) { acc += c.meanReduction(); ++nb; }
    }
    o.gr = nb ? acc / nb : 0; o.lat = c.latency();
    return o;
}
static Out run(int model, float k, const Vec& x, double sr, int block = 512, const Extras& e = Extras{}, double grFrom = -1) {
    return run(model, k, x, sr, block, e, [](Chain&, int) {}, nullptr, false, grFrom);
}
static double errDb(const Vec& y, const Vec& x, int lat, size_t from) {
    double e = 0, r = 0; for (size_t i = from + (size_t)lat; i < y.size(); ++i) { const double d = y[i] - x[i - (size_t)lat]; e += d * d; r += (double)x[i - (size_t)lat] * x[i - (size_t)lat]; }
    return 10 * std::log10(e / (r + 1e-30) + 1e-30);
}
static double rmsDb(const Vec& x, size_t a, size_t b) { double s = 0; b = std::min(b, x.size()); for (size_t i = a; i < b; ++i) s += (double)x[i] * x[i]; return 10 * std::log10(s / (double)std::max<size_t>(1, b - a) + 1e-20); }
static int peakPos(const Vec& y) { size_t pk = 0; for (size_t i = 0; i < y.size(); ++i) if (std::fabs(y[i]) > std::fabs(y[pk])) pk = i; return (int)pk; }
// spectre de puissance (Blackman-Harris), N puissance de 2
static std::vector<double> spectrum(const Vec& y, size_t start, size_t N) {
    std::vector<std::complex<double>> z(N);
    for (size_t i = 0; i < N; ++i) { const double a = 2 * kPiD * (double)i / (double)(N - 1); z[i] = (0.35875 - 0.48829 * std::cos(a) + 0.14128 * std::cos(2 * a) - 0.01168 * std::cos(3 * a)) * y[start + i]; }
    for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(z[i], z[j]); }
    for (size_t len = 2; len <= N; len <<= 1) { const auto wl = std::polar(1.0, -2 * kPiD / (double)len); for (size_t i = 0; i < N; i += len) { std::complex<double> w(1); for (size_t k = 0; k < len / 2; ++k) { const auto u = z[i + k], t = z[i + k + len / 2] * w; z[i + k] = u + t; z[i + k + len / 2] = u - t; w *= wl; } } }
    std::vector<double> p(N / 2); for (size_t k = 0; k < N / 2; ++k) p[k] = std::norm(z[k]); return p;
}
// YIN hors ligne (MIDI par trame de 240 échantillons, 0 si non voisé)
static Vec track(const Vec& x, double sr) {
    Vec o; const int W = 1200, tM = 700; std::vector<double> cm(tM + 2);
    for (size_t p = W + tM; p + W < x.size(); p += 240) {
        const float* a = &x[p]; double e = 0; for (int j = 0; j < W; ++j) e += a[j] * (double)a[j];
        if (std::sqrt(e / W) < 0.004) { o.push_back(0); continue; }
        double cum = 0; cm[0] = 1; for (int t = 1; t <= tM; ++t) { double d = 0; for (int j = 0; j < W; ++j) { const double v = a[j] - a[j - t]; d += v * v; } cum += d; cm[(size_t)t] = d * t / (cum + 1e-12); }
        int b = -1; for (int t = 40; t < tM; ++t) if (cm[(size_t)t] < 0.15) { while (t + 1 < tM && cm[(size_t)t + 1] < cm[(size_t)t]) ++t; b = t; break; }
        if (b < 0) { o.push_back(0); continue; }
        double tau = b; const double a0 = cm[(size_t)b - 1], b0 = cm[(size_t)b], c0 = cm[(size_t)b + 1], den = a0 - 2 * b0 + c0; if (std::fabs(den) > 1e-12) tau += 0.5 * (a0 - c0) / den;
        o.push_back((float)(69 + 12 * std::log2(sr / tau / 440)));
    }
    return o;
}

int main(int argc, char** argv) {
    const char* wav = nullptr; bool cpu = false, quick = false;
    for (int i = 1; i < argc; ++i) { if (!std::strcmp(argv[i], "--cpu")) cpu = true; else if (!std::strcmp(argv[i], "--rapide")) quick = true; else wav = argv[i]; }
    const double fs = 48000.0;

    // [1] Neutralité à 0 : sortie = entrée retardée de la latence déclarée (mono, stéréo différente G/D), aux trois fréquences
    for (double sr : {44100.0, 48000.0, 96000.0}) {
        if (quick && sr > 50000) continue;
        Vec x = voice(sr, 3.0, -20.0), xr = voice(sr, 3.0, -26.0, 210.0);
        for (size_t i = 0; i < x.size(); ++i) { x[i] += 0.003f * urnd(); xr[i] += 0.003f * urnd(); }
        for (int m = 0; m < 4; ++m) {
            auto o = run(m, 0.f, x, sr);
            auto s = run(m, 0.f, x, sr, 512, Extras{}, [](Chain&, int) {}, &xr);
            const double em = std::max(errDb(o.L, x, o.lat, 0), errDb(o.R, x, o.lat, 0));
            const double es = std::max(errDb(s.L, x, s.lat, 0), errDb(s.R, xr, s.lat, 0));
            std::printf("[1] %-10s %5.1f kHz k=0 : latence %d, écart au sec retardé mono %.1f dB, stéréo %.1f dB\n", nm(m), sr / 1000, o.lat, em, es);
            CHECK(em < -100.0 && es < -100.0, "k=0 non neutre (%s, %.0f Hz) : %.1f / %.1f dB", nm(m), sr, em, es);
        }
    }
    // [2] Latence déclarée = mesurée (impulsion faible), k = 0 et 0,25 ; 0 pour Gorge et Patine
    for (int m = 0; m < 4; ++m) for (float k : {0.f, 0.25f}) {
        Vec x(40000, 0.f); x[6000] = 0.01f;
        auto o = run(m, k, x, fs);
        const int meas = peakPos(o.L) - 6000;
        std::printf("[2] %-10s k=%.2f : latence déclarée %d, mesurée %d\n", nm(m), k, o.lat, meas);
        CHECK(meas == o.lat, "latence %s k=%.2f : mesurée %d, déclarée %d", nm(m), k, meas, o.lat);
        if (m == 1 || m == 3) CHECK(o.lat == 0, "%s doit être sans latence (%d)", nm(m), o.lat);
    }
    // [3] La compression suit le potard : réduction moyenne croissante, nulle à 0, franche à 10 (plus de plafond de -8 dB)
    {
        const Vec x = voice(fs, 6.0, -18.0);
        for (int m = 0; m < 4; ++m) {
            double g[6]; std::printf("[3] %-10s réduction moyenne (dB) à k =", nm(m));
            for (int i = 0; i <= 5; ++i) { g[i] = run(m, 0.2f * (float)i, x, fs, 512, Extras{}, 3.0).gr; std::printf(" %.1f:%.1f", 0.2 * i, g[i]); }
            std::printf("\n");
            CHECK(g[0] < 0.05, "%s : compression résiduelle à 0 (%.2f dB)", nm(m), g[0]);
            for (int i = 1; i <= 5; ++i) CHECK(g[i] >= g[i - 1] - 0.3, "%s : réduction non croissante (%.1f -> %.1f)", nm(m), g[i - 1], g[i]);
            CHECK(g[5] >= 9.0, "%s : réduction à 10 trop faible (%.1f dB)", nm(m), g[5]);
        }
    }
    // [4] Auto-entrée : même réduction (±1 dB) que la prise arrive à -30 ou à -6 dBFS
    for (int m = 0; m < 4; ++m) {
        double g[3]; int i = 0;
        for (double lv : {-18.0, -30.0, -6.0}) g[i++] = run(m, 0.6f, voice(fs, 8.0, lv), fs, 512, Extras{}, 5.0).gr;
        std::printf("[4] %-10s auto-entrée, k=6 : réduction %.2f dB à -18 ; %.2f à -30 ; %.2f à -6\n", nm(m), g[0], g[1], g[2]);
        CHECK(std::fabs(g[1] - g[0]) <= 1.0 && std::fabs(g[2] - g[0]) <= 1.0, "%s : auto-entrée dépendante du niveau", nm(m));
    }
    // [5] Bruit entre les phrases : phrases de 1,5 s, pauses de 3 s, bruit de pièce à -53 dBFS : bruit de pause <= +2 dB ; phrases ±3 dB
    {
        const double ph = 1.5, pa = 3.0; const int reps = 4;
        Vec x; for (int r = 0; r < reps; ++r) { auto v = voice(fs, ph, -26.0); x.insert(x.end(), v.begin(), v.end()); x.insert(x.end(), (size_t)(pa * fs), 0.f); }
        float lp = 0; for (auto& v : x) { lp += 0.3f * (urnd() - lp); v += 0.0045f * lp * 2.2f; }
        for (int m = 0; m < 4; ++m) for (float k : {0.5f, 1.f}) {
            auto o = run(m, k, x, fs);
            double nIn = 0, nOut = 0, vIn = 0, vOut = 0;
            for (int r = 1; r < reps; ++r) {
                const size_t t0 = (size_t)((r * (ph + pa)) * fs), a = (size_t)((r * (ph + pa) + ph + 2.0) * fs), b = (size_t)((r * (ph + pa) + ph + 3.0) * fs);
                nIn += rmsDb(x, a, b); nOut += rmsDb(o.L, a + (size_t)o.lat, b + (size_t)o.lat);
                vIn += rmsDb(x, t0, t0 + (size_t)(ph * fs)); vOut += rmsDb(o.L, t0 + (size_t)o.lat, t0 + (size_t)o.lat + (size_t)(ph * fs));
            }
            const double dn = (nOut - nIn) / (reps - 1), dv = (vOut - vIn) / (reps - 1);
            std::printf("[5] %-10s k=%.1f : bruit de pause %+.1f dB, phrases %+.1f dB (entrée : bruit %.1f dBFS)\n", nm(m), k, dn, dv, nIn / (reps - 1));
            CHECK(dn <= 2.0, "%s k=%.1f : bruit de pause remonté de %+.1f dB", nm(m), k, dn);
            CHECK(std::fabs(dv) <= 3.0, "%s k=%.1f : niveau des phrases %+.1f dB", nm(m), k, dv);
        }
    }
    // [6] Harmonies : Gorge et Patine sans moteur de hauteur (aucune raie hors des harmoniques de 220 Hz, < -90 dB) ;
    //     doubles de Grain Nord = unissons (rien à ±40-250 Hz des harmoniques : plus de quarte basse ni d'octave granulaires)
    {
        const Vec x = sine(fs, 3.0, 220.0, 0.2f);
        for (int m : {1, 3, 0}) {
            Extras e; e.option = 0.f;   // Gorge : sans râle (le râle ajoute volontairement f0/2)
            auto o = run(m, m == 0 ? 0.3f : 1.f, x, fs, 512, e);
            const auto P = spectrum(o.L, 65536, 65536); double pf = 0, worst = 0;
            for (size_t k = 1; k < P.size(); ++k) {
                const double f = (double)k * fs / 65536.0, h = f / 220.0, d = std::fabs(h - std::round(h)) * 220.0;
                if (std::round(h) == 1 && d < 15) pf += P[k];
                if (f < 60 || f > 4000 || d < (m == 0 ? 40.0 : 15.0)) continue;
                worst = std::max(worst, P[k]);
            }
            const double db = 10 * std::log10(worst / pf + 1e-30);
            std::printf("[6] %-10s k=%s : raie hors harmoniques la plus forte %.1f dB\n", nm(m), m == 0 ? "3" : "10", db);
            CHECK(db < (m == 0 ? -60.0 : -90.0), "%s : raies parasites %.1f dB", nm(m), db);
        }
        Extras e; e.option = 1.f;
        auto o = run(1, 1.f, voice(fs, 4.0, -18.0, 120.0, false), fs, 512, e);
        auto ref = run(1, 1.f, voice(fs, 4.0, -18.0, 120.0, false), fs, 512, Extras{[] { Extras z; z.option = 0.f; return z; }()});
        const auto P1 = spectrum(o.L, 96000, 65536), P0 = spectrum(ref.L, 96000, 65536);
        double s1 = 0, s0 = 0; for (size_t k = 0; k < P1.size(); ++k) { const double f = (double)k * fs / 65536.0; if (f > 50 && f < 70) { s1 += P1[k]; s0 += P0[k]; } }
        std::printf("[6] Gorge râle : énergie autour de f0/2 (60 Hz) %+.1f dB par rapport au réglage sans râle\n", 10 * std::log10(s1 / (s0 + 1e-30)));
        CHECK(s1 > s0 * 10.0, "le râle de Gorge ne produit pas de sous-harmonique");
    }
    // [7] Pas de marche sur la course : balayage 0 -> 10 en 12 s sur un son stationnaire ; énergie > 10 kHz et totale par 50 ms
    for (int m = 0; m < 4; ++m) {
        Vec x = voice(fs, 13.0, -18.0, 180.0, false); for (auto& v : x) v += 0.004f * urnd();
        auto o = run(m, 0.f, x, fs, 480, Extras{}, [&](Chain& c, int bi) { c.setKnob(std::min(1.f, (float)bi * 480.f / (float)(12.0 * fs)), voicings()[(size_t)m]); });
        const size_t W = 2400; double worst = 0, wk = 0, prevH = 0, prevT = 0, at[3] = {0, 0, 0};
        Bq hp; hp.hp(fs, 10000.0, 0.7071); Bq hp2; hp2.hp(fs, 10000.0, 0.7071); Vec h(o.L.size()); for (size_t i = 0; i < h.size(); ++i) h[i] = hp2.run(hp.run(o.L[i]));
        for (size_t f = 1; f * W + W < o.L.size(); ++f) {
            const double eh = rmsDb(h, f * W, f * W + W), et = rmsDb(o.L, f * W, f * W + W), kk = (double)(f * W) / (12.0 * fs);
            if (f > 1) { const double d = std::max(std::fabs(eh - prevH), std::fabs(et - prevT)); if (d > worst) { worst = d; wk = kk; }
                for (int j = 0; j < 3; ++j) { const double kt = j == 0 ? 0.86 : j == 1 ? 0.89 : 0.91; if (std::fabs(kk - kt) < 0.012) at[j] = std::max(at[j], d); } }
            prevH = eh; prevT = et;
        }
        std::printf("[7] %-10s balayage : plus grand saut entre trames de 50 ms %.2f dB (à k=%.2f) ; autour de 8,6/8,9/9,1 : %.2f/%.2f/%.2f dB\n", nm(m), worst, wk * 10, at[0], at[1], at[2]);
        CHECK(worst < 3.0, "%s : marche de %.1f dB sur la course", nm(m), worst);
    }
    // [8] Repliement en fin de course : sinus de 3150 Hz, énergie sous 1,9 kHz (hors continu) < -80 dB, à 44,1 et 48 kHz.
    //     Poupée exclue : TuneCore prend ce sinus pur pour une voix à f0/4 ou f0/3 (787 ou 1050 Hz) et la pédale/l'octave le
    //     transposent (traitement de hauteur voulu, pas repliement) ; ses saturations sont suréchantillonnées ×4 (Saturation.h).
    for (double sr : {44100.0, 48000.0}) for (int m : {0, 1, 3}) {
        const Vec x = sine(sr, 3.5, 3150.0, 0.3f);
        auto o = run(m, 1.f, x, sr);
        const auto P = spectrum(o.L, (size_t)(1.5 * sr), 65536); double pf = 0, low = 0;
        for (size_t k = 1; k < P.size(); ++k) { const double f = (double)k * sr / 65536.0; pf = std::max(pf, P[k]); if (f > 40 && f < 1900) low += P[k]; }
        const double db = 10 * std::log10(low / pf + 1e-30);
        std::printf("[8] %-10s %.1f kHz k=10 : énergie sous 1,9 kHz %.1f dB\n", nm(m), sr / 1000, db);
        CHECK(db < -80.0, "%s : repliement %.1f dB", nm(m), db);
    }
    // [9] Hauteur : glissando A3 -> B3 (400 ms), Poupée à 10 : dépassement < 20 cents ; voix accordée laissée intacte à k bas
    {
        Vec x; double ph = 0;
        for (int i = 0; i < (int)(1.8 * fs); ++i) { const double t = i / fs, mm = t < 0.7 ? 57 : t < 1.1 ? 57 + 2 * (t - 0.7) / 0.4 : 59; ph += 440 * std::pow(2, (mm - 69) / 12) / fs; ph -= std::floor(ph);
            double s = 0; for (int h = 1; h <= 8; ++h) s += std::sin(2 * kPiD * h * ph) / h; x.push_back(0.12f * (float)s); }
        auto o = run(2, 1.f, x, fs); auto a = track(o.L, fs);
        double mx = 0; for (size_t i = 0; i < a.size(); ++i) { const double t = ((double)i * 240 + 1900 - o.lat) / fs; if (t > 1.0 && t < 1.7 && a[i] > 0) mx = std::max(mx, (double)a[i]); }
        std::printf("[9] Poupée k=10, glissando A3->B3 : hauteur max après l'arrivée %.3f (dépassement %.1f cents)\n", mx, (mx - 59) * 100);
        CHECK(mx > 0 && (mx - 59) * 100 < 20.0, "dépassement %.1f cents", (mx - 59) * 100);
        // mélodie légèrement fausse (+-35 cents) : Poupée la plaque, Grain Nord à 3 la laisse (tonalité pas encore utilisée)
        Vec y; ph = 0; const int mel[] = {57, 60, 62, 64, 65, 64, 62, 60, 57, 59, 60, 62, 64, 62, 60, 59};
        for (int r = 0; r < 2; ++r) for (int q = 0; q < 16; ++q) { const double c = ((q * 7) % 9 - 4) * 8.75; for (int i = 0; i < (int)(0.35 * fs); ++i) { ph += 440 * std::pow(2, (mel[q] + c / 100 - 69) / 12) / fs; ph -= std::floor(ph);
            double s = 0; for (int h = 1; h <= 8; ++h) s += std::sin(2 * kPiD * h * ph) / h; y.push_back(0.12f * (float)s * (float)std::min(1.0, i / 600.0)); } }
        auto err = [&](const Vec& z, int lat) { auto b = track(z, fs); double s = 0; int n = 0; for (size_t i = (size_t)(lat / 240) + 20; i < b.size(); ++i) if (b[i] > 0) { s += std::fabs(b[i] - std::round(b[i])) * 100; ++n; } return n ? s / n : 99.0; };
        const double e0 = err(y, 0); auto pu = run(2, 1.f, y, fs); auto g3 = run(0, 0.3f, y, fs);
        const double ep = err(pu.L, pu.lat), eg = err(g3.L, g3.lat);
        std::printf("[9] mélodie à ±35 c : écart moyen à la grille %.1f c en entrée ; Poupée 10 : %.1f c ; Grain Nord 3 : %.1f c\n", e0, ep, eg);
        CHECK(ep < 5.0, "Poupée ne plaque pas la hauteur (%.1f c)", ep);
        CHECK(std::fabs(eg - e0) < 3.0, "Grain Nord à 3 modifie la hauteur (%.1f -> %.1f c)", e0, eg);
    }
    // [10] NaN/inf en entrée : sortie finie, le son revient 100 ms après
    for (int m = 0; m < 4; ++m) {
        Vec x = voice(fs, 3.0, -18.0); for (int i = 0; i < 64; ++i) x[48000 + (size_t)i * 7] = i % 2 ? NAN : INFINITY;
        auto o = run(m, 0.7f, x, fs); bool bad = false; for (size_t i = 0; i < o.L.size(); ++i) if (!std::isfinite(o.L[i]) || !std::isfinite(o.R[i])) bad = true;
        const double after = rmsDb(o.L, (size_t)(1.2 * fs), (size_t)(2.9 * fs)), before = rmsDb(o.L, (size_t)(0.3 * fs), (size_t)(0.99 * fs));
        std::printf("[10] %-10s NaN/inf en entrée : %s, niveau après %.1f dB (avant %.1f dB)\n", nm(m), bad ? "NaN EN SORTIE" : "sortie finie", after, before);
        CHECK(!bad && after > before - 6.0, "%s : NaN ou son perdu après une entrée non finie", nm(m));
    }
    // [11] Zéro allocation en temps réel (réglages qui bougent, tonalités, moteur Classique <-> Actuel) ; blocs 1 à 8192 identiques
    for (int m = 0; m < 4; ++m) {
        Chain c; c.select(m); c.prepare(fs, 512);
        Vec a = voice(fs, 4.0, -18.0), L(a.size()), R(a.size()); bool bad = false;
        gAllocs = 0; gCountAlloc = true;
        for (size_t p = 0, blk = 0; p < a.size(); p += 512, ++blk) {
            Extras e; e.trimDb = (float)(blk % 3) * 6.f - 6.f; e.toneDb = (float)(blk % 5) - 2.f; e.mix = (float)(blk % 4) / 3.f; e.outDb = (float)(blk % 2) * 6.f;
            e.special = (float)(blk % 3) / 2.f; e.autoIn = blk % 7 != 0; e.autoGain = blk % 5 != 0; e.option = (float)(blk % 2);
            c.setExtras(e); c.setKeySel((int)(blk % 13), (int)(blk % 3)); c.setClassic(blk % 40 >= 30); c.setKnob((float)(blk % 11) / 10.f, voicings()[(size_t)m]);
            c.process(&a[p], nullptr, &L[p], &R[p], (int)std::min<size_t>(512, a.size() - p));
        }
        gCountAlloc = false;
        for (size_t i = 0; i < L.size(); ++i) if (!std::isfinite(L[i]) || !std::isfinite(R[i]) || std::fabs(L[i]) > 4.f) bad = true;
        std::printf("[11] %-10s réglages extrêmes et bascules : %s, %ld allocation(s)\n", nm(m), bad ? "SORTIE INVALIDE" : "sortie finie", gAllocs);
        CHECK(!bad && gAllocs == 0, "%s : sortie invalide ou allocation en temps réel (%ld)", nm(m), gAllocs);
        const Vec x = voice(fs, 2.0, -18.0); auto ref = run(m, 0.7f, x, fs, 512); double worst = -400;
        for (int b : {1, 7, 64, 333, 2048, 8192}) { if (quick && (b == 1 || b == 8192)) continue; auto o = run(m, 0.7f, x, fs, b); worst = std::max(worst, errDb(o.L, ref.L, 0, 0)); }
        std::printf("[11] %-10s blocs 1 à 8192 : écart max à la référence (512) %.1f dB\n", nm(m), worst);
        CHECK(worst < -100.0, "%s : la sortie dépend de la taille des blocs (%.1f dB)", nm(m), worst);
    }
    // [12] Mix 0 = sec retardé exact ; bypass (même Fader que le processeur) exact et sans clic
    for (int m = 0; m < 4; ++m) {
        Extras e; e.mix = 0.f; e.toneDb = 4.f; e.special = 1.f; const Vec a = voice(fs, 2.0, -18.0), b = voice(fs, 2.0, -20.0, 230.0);
        auto o = run(m, 0.8f, a, fs, 480, e, [](Chain&, int) {}, &b);
        const double db = std::max(errDb(o.L, a, o.lat, 0), errDb(o.R, b, o.lat, 0));
        Chain c; Vec sc(4096); c.select(m);
        auto bm = bypassfx::measure(fs, [&] { Chain t; t.select(m); return t.latency(); }(), 512, [&] { c.prepare(fs, 512); }, [&](float* l, float* r, int n) { c.setKnob(0.6f, voicings()[(size_t)m]); c.process(l, r, l, r ? r : sc.data(), n); });
        // saut au basculement : < -60 dBFS, ou pas plus grand que la plus forte pente du signal traité lui-même (harmoniques ajoutées)
        std::printf("[12] %-10s mix 0 : écart %.1f dB ; bypass : erreur %.1f dB, saut au basculement %.1f dBFS (pente max du signal seul %.1f)\n", nm(m), db, bm.exactDb, bm.jumpDb, bm.plainDb);
        CHECK(db < -120.0 && bm.exactDb < -120.0 && bm.jumpDb < std::max(-60.0, bm.plainDb + 0.5), "%s : mix 0 / bypass non exacts", nm(m));
    }
    // [13] Compatibilité : le moteur Classique de la chaîne = l'ancienne chaîne (classic::Chain) à l'échantillon près, latence d'origine
    for (int m = 0; m < 4; ++m) {
        const Vec x = voice(fs, 3.0, -18.0);
        auto o = run(m, 0.5f, x, fs, 512, Extras{}, [](Chain&, int) {}, nullptr, true);
        classic::Chain c; c.prepare(fs, 512); c.setExtras(Extras{}); Vec L(x.size()), R(x.size());
        for (size_t p = 0; p < x.size(); p += 512) { c.setKnob(0.5f, classic::voicings()[(size_t)m]); c.process(&x[p], nullptr, &L[p], &R[p], (int)std::min<size_t>(512, x.size() - p)); }
        const double db = errDb(o.L, L, 0, 0);
        std::printf("[13] %-10s Classique : latence %d (d'origine 814 à 48 kHz), écart à l'ancienne chaîne %.1f dB\n", nm(m), o.lat, db);
        CHECK(o.lat == c.latency() && db < -200.0, "%s : le mode Classique ne reproduit pas l'ancienne chaîne (%.1f dB)", nm(m), db);
    }
    // [14] CPU par instance (48 kHz, blocs de 256), voix de synthèse, potard à 5 et à 10
    if (cpu) for (int m = 0; m < 4; ++m) for (float k : {0.5f, 1.f}) {
        const Vec x = voice(fs, 10.0, -18.0); Vec L(x.size()), R(x.size());
        Chain c; c.select(m); c.prepare(fs, 256); double best = 1e9;
        for (int rep = 0; rep < 3; ++rep) {
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t p = 0; p < x.size(); p += 256) { c.setKnob(k, voicings()[(size_t)m]); c.process(&x[p], nullptr, &L[p], &R[p], (int)std::min<size_t>(256, x.size() - p)); }
            best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
        }
        const double pc = 100.0 * best / 10.0;
        std::printf("[14] %-10s k=%.0f : %.2f %% d'un cœur\n", nm(m), k * 10, pc);
        // objectif < 7 % ; Poupée : TuneCore (détection + LPC des formants) coûte à lui seul ~4,5-5,5 % : borne à 9 %
        const double lim = m == 2 ? 9.0 : 7.0;
        CHECK(pc < lim, "%s : CPU %.2f %% (objectif < %.0f %%)", nm(m), pc, lim);
    }
    // [15] Vraie prise (si fournie) : niveau tenu, pas de pic > 1, caractère attendu
    Vec in; double sr = 44100;
    if (wav && readWav(wav, in, sr)) {
        std::printf("[15] prise : %.1f s à %.0f Hz\n", (double)in.size() / sr, sr);
        for (int m = 0; m < 4; ++m) for (float k : {0.f, 0.5f, 1.f}) {
            auto o = run(m, k, in, sr); float pk = 0; bool bad = false;
            for (size_t i = 0; i < o.L.size(); ++i) { if (!std::isfinite(o.L[i])) bad = true; pk = std::max(pk, std::fabs(o.L[i])); }
            const double d = rmsDb(o.L, (size_t)o.lat, o.L.size()) - rmsDb(in, 0, in.size() - (size_t)o.lat);
            std::printf("     %-10s k=%.0f : niveau %+5.1f dB, crête %.2f\n", nm(m), k * 10, d, pk);
            CHECK(!bad && pk <= 1.0f && std::fabs(d) < 2.5, "%s k=%.0f : niveau %+.1f dB ou crête %.2f", nm(m), k * 10, d, pk);
            if (k > 0.f) { char fn[160]; std::snprintf(fn, sizeof fn, "/tmp/onek_%s_%d.raw", voicings()[(size_t)m].id, (int)(k * 10)); if (FILE* f = std::fopen(fn, "wb")) { std::fwrite(o.L.data(), 4, o.L.size(), f); std::fclose(f); } }
        }
    } else std::printf("[15] pas de prise WAV fournie : test ignoré\n");
    std::printf(fails ? "\n%d ECHEC(S)\n" : "\nTESTS UN-POTARD OK\n", fails);
    return fails ? 1 : 0;
}
