// Tests des plugins à un potard (hors JUCE). Usage : onek_test [prise.wav]
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include "../Source/OneKnob/OneKnob.h"
#include "../Source/Kit/BypassFadeTest.h"
#include <new>
#include <cstdlib>
using namespace onek;
static int fails = 0;
// compteur d'allocations (test « zéro allocation en temps réel »)
static bool gCountAlloc = false;
static long gAllocs = 0;
void* operator new(std::size_t n) { if (gCountAlloc) ++gAllocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { if (gCountAlloc) ++gAllocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("  ECHEC: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// lecteur WAV PCM16/24/32/float -> mono
static bool readWav(const char* path, std::vector<float>& mono, double& sr) {
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

// énergie par bande (FFT radix-2 sur trames de 2048)
static void bands(const std::vector<float>& x, double sr, double& low, double& mid, double& high, double& centroid) {
    const int N = 2048; low = mid = high = 0; double num = 0, den = 0;
    for (size_t s = (size_t)sr; s + N < x.size(); s += N) {
        std::vector<std::complex<double>> a(N);
        for (int i = 0; i < N; ++i) a[(size_t)i] = x[s + (size_t)i] * (0.5 - 0.5 * std::cos(2 * 3.14159265 * i / N));
        for (int i = 1, j = 0; i < N; ++i) { int bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[(size_t)i], a[(size_t)j]); }
        for (int len = 2; len <= N; len <<= 1) { const double ang = -2 * 3.14159265 / len; std::complex<double> wl(std::cos(ang), std::sin(ang));
            for (int i = 0; i < N; i += len) { std::complex<double> w(1); for (int k = 0; k < len / 2; ++k) { auto u = a[(size_t)(i + k)], v = a[(size_t)(i + k + len / 2)] * w; a[(size_t)(i + k)] = u + v; a[(size_t)(i + k + len / 2)] = u - v; w *= wl; } } }
        for (int k = 1; k < N / 2; ++k) { const double f = k * sr / N, e = std::norm(a[(size_t)k]); (f < 250 ? low : f < 4000 ? mid : high) += e; num += f * e; den += e; }
    }
    centroid = den > 0 ? num / den : 0;
}
static double rms(const std::vector<float>& x) { double s = 0; for (float v : x) s += (double)v * v; return std::sqrt(s / (double)std::max<size_t>(1, x.size())); }

static std::vector<float> run(const std::vector<float>& in, double sr, int vi, float k) {
    Chain c; c.prepare(sr, 512);
    const auto& v = voicings()[(size_t)vi];
    std::vector<float> out(in.size()), L(512), R(512);
    for (size_t p = 0; p < in.size(); p += 512) {
        const int n = (int)std::min<size_t>(512, in.size() - p);
        c.setKnob(k, v);
        c.process(&in[p], nullptr, L.data(), R.data(), n);
        for (int i = 0; i < n; ++i) out[p + (size_t)i] = 0.5f * (L[(size_t)i] + R[(size_t)i]);
    }
    return out;
}

int main(int argc, char** argv) {
    // 1. tonalité automatique : mélodie en ré mineur, notes un peu fausses -> verrouillage fiable (ré mineur ou son relatif fa majeur)
    {
        const double sr = 48000; std::vector<float> x; float ph = 0;
        const int seq[] = {62, 64, 65, 62, 69, 67, 65, 64, 62, 65, 69, 70, 69, 67, 65, 64, 62, 60, 62, 65, 67, 69, 65, 62};
        for (int rep = 0; rep < 2; ++rep) for (int m : seq) {
            const float f = 440.f * std::pow(2.f, ((float)m - 69.f + 0.18f) / 12.f);
            for (int i = 0; i < (int)(0.4 * sr); ++i) { ph += f / (float)sr; ph -= std::floor(ph); const float t = (float)i / (float)(0.4 * sr);
                x.push_back(0.4f * (std::sin(6.2832f * ph) + 0.4f * std::sin(12.566f * ph) + 0.2f * std::sin(18.85f * ph)) * std::min(1.f, 30.f * t) * (1.f - 0.3f * t)); }
        }
        voile::Engine e; e.prepare(sr); voile::Params p; p.safe = true; p.autoKey = p.autoScale = true; p.tension = 1.f; e.set(p);
        std::vector<float> a(x.size()), b(x.size());
        for (size_t i = 0; i < x.size(); i += 512) { const int n = (int)std::min<size_t>(512, x.size() - i); e.process(&x[i], nullptr, &a[i], &b[i], n); }
        std::printf("[1] tonalité détectée : %s (%d/%d)\n", e.keyKnown() ? "oui" : "non", e.detectedKey(), e.detectedScale());
        const bool dmin = e.detectedKey() == 2 && e.detectedScale() == 2, fmaj = e.detectedKey() == 5 && e.detectedScale() == 1;
        CHECK(e.keyKnown() && e.keyStatus() == 1 && (dmin || fmaj), "attendu ré mineur (2/2) ou fa majeur (5/1), obtenu %d/%d", e.detectedKey(), e.detectedScale());
    }
    // 1b. justesse mesurée (YIN hors-ligne) : la correction ne doit jamais désaccorder la prise
    {
        const double sr = 48000;
        struct Nt { int m; float c; };
        auto synth = [&](const std::vector<Nt>& v, float dur) { std::vector<float> x; double ph = 0;
            for (auto& nt : v) { const int n = (int)(dur * sr);
                for (int i = 0; i < n; ++i) { const float f = 440.f * std::pow(2.f, ((float)nt.m - 69.f + nt.c / 100.f) / 12.f); ph += f / sr; ph -= std::floor(ph);
                    float sm = 0; for (int h = 1; h <= 8; ++h) sm += std::sin(6.2832 * h * ph) / (float)h;
                    const float t = (float)i / (float)n; x.push_back(0.3f * sm * std::min(1.f, t * (float)n / (0.02f * (float)sr)) * std::min(1.f, (1 - t) * (float)n / (0.03f * (float)sr))); }
                for (int i = 0; i < (int)(0.08 * sr); ++i) x.push_back(0.f); }
            return x; };
        auto track = [&](const std::vector<float>& x) { std::vector<float> o; const int W = 1024, tMax = 343, hop = 480;
            std::vector<float> d(x.size() / 2); for (size_t i = 0; i < d.size(); ++i) d[i] = 0.5f * (x[2 * i] + x[2 * i + 1]);
            std::vector<float> cm((size_t)tMax + 2);
            for (size_t p = 0; p + hop < x.size(); p += hop) { o.push_back(0); const long s = (long)(p / 2) - W / 2 - tMax; if (s < 0 || s + W + tMax > (long)d.size()) continue;
                const float* a = &d[(size_t)s + tMax]; double e = 0; for (int j = 0; j < W; ++j) e += a[j] * a[j]; if (std::sqrt(e / W) < 0.004) continue;
                double cum = 0; cm[0] = 1; for (int t = 1; t <= tMax; ++t) { double dd = 0; for (int j = 0; j < W; ++j) { double v = a[j] - a[j - t]; dd += v * v; } cum += dd; cm[(size_t)t] = cum > 1e-12 ? (float)(dd * t / cum) : 1.f; }
                int best = -1; for (int t = 26; t < tMax; ++t) if (cm[(size_t)t] < 0.12f) { while (t + 1 < tMax && cm[(size_t)t + 1] < cm[(size_t)t]) ++t; best = t; break; }
                if (best < 1) continue; double tau = best, a0 = cm[(size_t)best - 1], b0 = cm[(size_t)best], c0 = cm[(size_t)best + 1], den = a0 - 2 * b0 + c0; if (std::fabs(den) > 1e-12) tau += 0.5 * (a0 - c0) / den;
                o.back() = (float)(69 + 12 * std::log2(24000.0 / tau / 440.0)); }
            return o; };
        auto runK = [&](const std::vector<float>& x, int vi, float k, int ks, int ss, int& lat, int& st) { Chain c; c.prepare(sr, 512); c.setKeySel(ks, ss); std::vector<float> o(x.size()), L(512), R(512);
            for (size_t p = 0; p < x.size(); p += 512) { const int n = (int)std::min<size_t>(512, x.size() - p); c.setKnob(k, voicings()[(size_t)vi]); c.process(&x[p], nullptr, L.data(), R.data(), n); for (int i = 0; i < n; ++i) o[p + (size_t)i] = 0.5f * (L[(size_t)i] + R[(size_t)i]); }
            lat = c.latency(); st = c.keyStatus(); return o; };
        // part des trames où |sortie - entrée| > 10 cents
        auto over = [&](const std::vector<float>& in, const std::vector<float>& out, int lat) { auto a = track(in), b = track(out); const size_t L = (size_t)std::lround((double)lat / 480.); int n = 0, bad = 0;
            for (size_t i = 0; i + L < a.size() && i + L < b.size(); ++i) { if (a[i] <= 0 || b[i + L] <= 0) continue; const double c = (b[i + L] - a[i]) * 100; if (std::fabs(c) > 600) continue; ++n; if (std::fabs(c) > 10) ++bad; } return n ? 100.0 * bad / n : 0.0; };
        auto offGrid = [&](const std::vector<float>& out, int key, int sc) { auto b = track(out); int n = 0, bad = 0; for (float m : b) { if (m <= 0) continue; ++n; if (std::fabs(m - (float)voile::nearestAllowed(m, key, sc)) * 100 > 10) ++bad; } return n ? 100.0 * bad / n : 0.0; };
        const int dm[] = {62, 64, 65, 62, 69, 67, 65, 64, 62, 65, 69, 70, 69, 67, 65, 64, 62, 60, 62, 65, 67, 69, 65, 62};
        std::vector<Nt> tuned; for (int r = 0; r < 3; ++r) for (int m : dm) tuned.push_back({m, (float)(((r * 7 + m) % 9) - 4)});    // +-4 cents
        std::vector<Nt> rap; unsigned sd = 12345; for (int i = 0; i < 50; ++i) { sd = sd * 1103515245u + 12345u; rap.push_back({55 + (int)((sd >> 16) % 22), 0.f}); }
        auto xt = synth(tuned, 0.4f), xr = synth(rap, 0.35f);
        int lat, st;
        for (int vi : {0, 2}) {
            auto y = runK(xt, vi, 0.1f, 0, 0, lat, st); const double lo = over(xt, y, lat);
            std::printf("[1b] %s k=0.1 prise accordée, Auto : %.0f%% des trames à >10 cents de l'entrée\n", voicings()[(size_t)vi].name, lo);
            CHECK(lo <= 3.0, "prise accordée désaccordée à k bas (%s) : %.0f%%", voicings()[(size_t)vi].name, lo);
            y = runK(xr, vi, 1.0f, 0, 0, lat, st); const double ch = over(xr, y, lat);
            std::printf("[1b] %s k=1 prise hors gamme, Auto non fiable (état %d) : %.0f%% à >10 cents de l'entrée\n", voicings()[(size_t)vi].name, st, ch);
            CHECK(st == 2 && ch <= 4.0, "Auto non fiable : la hauteur doit rester celle de l'entrée (%s) état %d, %.0f%%", voicings()[(size_t)vi].name, st, ch);
        }
        { auto y = runK(xt, 2, 1.0f, 7, 1, lat, st);   // Poupée à fond, tonalité manuelle fausse : Fa# majeur
          const double og = offGrid(y, 6, 1); std::printf("[1b] Poupée k=1, manuel Fa# majeur (faux) : %.0f%% des trames hors de la grille choisie (état %d)\n", og, st);
          CHECK(st == 0 && og <= 25.0, "tonalité manuelle : la sortie doit suivre la grille choisie (%.0f%% hors grille)", og); }
    }
    // 1c. bypass honoré (même Fader que OneKnobProcessor::processBlock) : sortie = entrée retardée (< -120 dB), saut au basculement < -60 dBFS
    for (size_t vi = 0; vi < voicings().size(); ++vi) {
        Chain c; std::vector<float> sc(4096); const auto& v = voicings()[vi];
        auto m = bypassfx::measure(48000.0, c.latency(), 512, [&] { c.prepare(48000.0, 512); }, [&](float* l, float* r, int n) { c.setKnob(0.6f, v); c.process(l, r, l, r ? r : sc.data(), n); });
        std::printf("[1c] bypass %s : latence %d, erreur %.1f dB, saut au basculement %.1f dBFS (signal seul : %.1f)\n", v.name, c.latency(), m.exactDb, m.jumpDb, m.plainDb);
        CHECK(m.exactDb < -120.0, "bypass non exact (%s) : %.1f dB", v.name, m.exactDb); CHECK(m.jumpDb < -60.0, "clic au basculement (%s) : %.1f dBFS", v.name, m.jumpDb);
    }
    { // [1d] chantier 12 : latence mesurée = déclarée (suréchantillonnage de la saturation inclus) ; repliement du limiteur doux
        for (auto& v : voicings()) {
            Chain c; c.prepare(48000.0, 512); c.setKnob(0.5f, v);
            std::vector<float> in(30000, 0.f), L(in.size()), R(in.size()); in[4000] = 0.01f;   // impulsion faible : saturation quasi linéaire
            for (size_t i = 0; i < in.size(); i += 500) c.process(&in[i], nullptr, &L[i], &R[i], 500);
            size_t pk = 0; for (size_t i = 0; i < L.size(); ++i) if (std::fabs(L[i]) > std::fabs(L[pk])) pk = i;
            std::printf("[1d] %s : latence déclarée %d, mesurée %d\n", v.name, c.latency(), (int)pk - 4000);
            CHECK(std::abs((int)pk - 4000 - c.latency()) <= 2, "latence %s : mesurée %d, déclarée %d", v.name, (int)pk - 4000, c.latency());
        }
        const double fs = 48000.0, f0 = 5000.0; const size_t N = 65536;
        std::vector<float> y(N * 2); TanhAdaa lim;
        for (size_t n = 0; n < y.size(); ++n) y[n] = lim.process((float)(0.9 * std::sin(2.0 * 3.14159265358979 * f0 * (double)n / fs)) * 0.9f) / 0.9f * 0.95f;
        std::vector<std::complex<double>> z(N);
        for (size_t i = 0; i < N; ++i) { const double w = 0.35875 - 0.48829 * std::cos(2 * 3.14159265358979 * (double)i / (double)(N - 1)) + 0.14128 * std::cos(4 * 3.14159265358979 * (double)i / (double)(N - 1)) - 0.01168 * std::cos(6 * 3.14159265358979 * (double)i / (double)(N - 1)); z[i] = w * y[N + i]; }
        for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(z[i], z[j]); }
        for (size_t len = 2; len <= N; len <<= 1) { const std::complex<double> wl = std::polar(1.0, -2 * 3.14159265358979 / (double)len); for (size_t i = 0; i < N; i += len) { std::complex<double> w(1); for (size_t k = 0; k < len / 2; ++k) { const auto u = z[i + k], t = z[i + k + len / 2] * w; z[i + k] = u + t; z[i + k + len / 2] = u - t; w *= wl; } } }
        double pf = 0, worst = 0;
        for (size_t k = 1; k < N / 2; ++k) { const double f = (double)k * fs / (double)N, p = std::norm(z[k]); const int h = (int)std::lround(f / f0); if (std::fabs(f - h * f0) < 60.0) { if (h == 1) pf += p; continue; } if (f > 20000.0) continue; worst = std::max(worst, p); }
        const double db = 10 * std::log10(worst / pf + 1e-30);
        std::printf("[1d] limiteur doux (tanh ADAA), sinus 5 kHz à 0,9 : repliement %.1f dB (tanh direct : -79 dB)\n", db);
        CHECK(db < -85.0, "repliement du limiteur %.1f dB", db);
    }
    // [3] volet avancé (chantier 8)
    {
        const double fs = 48000.0;
        // voix synthétique : 5 harmoniques, syllabes de 180 ms (enveloppe), RMS ramené à -18 dBFS
        auto voice = [&](double secs, float gainDb) {
            std::vector<float> x((size_t)(secs * fs)); double ph = 0, s2 = 0;
            for (size_t i = 0; i < x.size(); ++i) {
                const double t = (double)i / fs, f = 196.0 * (1.0 + 0.03 * std::sin(2 * 3.14159265 * 0.7 * t));
                ph += f / fs; ph -= std::floor(ph);
                double v = 0; for (int h = 1; h <= 5; ++h) v += std::sin(2 * 3.14159265 * h * ph) / h;
                const double syl = std::fmod(t, 0.24) < 0.18 ? std::sin(3.14159265 * std::fmod(t, 0.24) / 0.18) : 0.0;
                x[i] = (float)(v * syl); s2 += x[i] * (double)x[i];
            }
            const float k = (float)(std::pow(10.0, (-18.0 + gainDb) / 20.0) / std::sqrt(s2 / (double)x.size()));
            for (auto& v : x) v *= k;
            return x;
        };
        // réduction de gain moyenne sur les 2 dernières secondes (6 s de signal)
        auto grOf = [&](const Voicing& v, float levelDb, float trimDb, bool autoIn) {
            auto x = voice(6.0, levelDb);
            Chain c; c.prepare(fs, 512); Extras e; e.trimDb = trimDb; e.autoIn = autoIn; c.setExtras(e);
            std::vector<float> L(512), R(512); double acc = 0; int nb = 0;
            for (size_t p = 0; p + 512 <= x.size(); p += 512) {
                c.setKnob(0.6f, v); c.process(&x[p], nullptr, L.data(), R.data(), 512);
                if ((double)p > 4.0 * fs) { acc += c.meanReduction(); ++nb; }
            }
            return acc / std::max(1, nb);
        };
        for (auto& v : voicings()) {
            const double g0 = grOf(v, 0, 0, true), gm = grOf(v, -12, 0, true), gp = grOf(v, 12, 0, true);
            const double tm = grOf(v, 0, -12, true), tp = grOf(v, 0, 12, true);
            const double om = grOf(v, -12, 0, false), op = grOf(v, 12, 0, false);
            std::printf("[3a] %s auto-entrée : réduction %.2f dB à -18 dBFS ; entrée -12/+12 dB : %.2f / %.2f ; trim -12/+12 : %.2f / %.2f (sans auto : %.2f / %.2f)\n",
                        v.name, g0, gm, gp, tm, tp, om, op);
            for (double gg : {gm, gp, tm, tp}) CHECK(std::fabs(gg - g0) <= 1.0, "auto-entrée %s : réduction %.2f dB au lieu de %.2f ±1", v.name, gg, g0);
        }
        // mix 0 = sec retardé de la latence, exact
        for (auto& v : voicings()) {
            Chain c; c.prepare(fs, 512); Extras e; e.mix = 0.f; e.toneDb = 4.f; e.special = 1.f; c.setExtras(e);
            const int n = 48000; std::vector<float> a(n), b(n), L(n), R(n); unsigned sd = 7;
            for (int i = 0; i < n; ++i) { sd = sd * 1664525u + 1013904223u; a[(size_t)i] = 0.4f * ((float)(sd >> 8) / 16777216.f - 0.5f); b[(size_t)i] = 0.3f * std::sin(0.01f * (float)i); }
            for (int p = 0; p < n; p += 480) { c.setKnob(0.8f, v); c.process(&a[(size_t)p], &b[(size_t)p], &L[(size_t)p], &R[(size_t)p], 480); }
            double err = 0, ref = 0; const int lat = c.latency();
            for (int i = lat; i < n; ++i) { err += std::pow(L[(size_t)i] - a[(size_t)(i - lat)], 2) + std::pow(R[(size_t)i] - b[(size_t)(i - lat)], 2); ref += a[(size_t)i] * (double)a[(size_t)i]; }
            const double db = 10 * std::log10(err / ref + 1e-30);
            std::printf("[3b] %s mix 0 : écart au sec retardé de %d échantillons %.1f dB\n", v.name, lat, db);
            CHECK(db < -120.0, "mix 0 (%s) : %.1f dB", v.name, db);
        }
        // pas de NaN, latence inchangée par le volet, aucune allocation dans process()
        for (auto& v : voicings()) {
            Chain c; c.prepare(fs, 512);
            const int n = 96000; std::vector<float> a(n), L(n), R(n); unsigned sd = 99;
            for (int i = 0; i < n; ++i) { sd = sd * 1664525u + 1013904223u; const float w = (float)(sd >> 8) / 16777216.f - 0.5f;
                a[(size_t)i] = i < 20000 ? 1.8f * w : i < 40000 ? 0.f : i < 60000 ? 0.9f : 0.5f * std::sin(0.3f * (float)i) + 1e-30f * w; }
            bool bad = false; gAllocs = 0; gCountAlloc = true;
            for (int p = 0, blk = 0; p < n; p += 512, ++blk) {
                Extras e; e.trimDb = (blk % 3 - 1) * 12.f; e.toneDb = (blk % 5 - 2) * 3.f; e.mix = (float)(blk % 4) / 3.f; e.outDb = (blk % 2) * 12.f;
                e.special = (float)(blk % 3) / 2.f; e.autoIn = blk % 7 != 0; e.autoGain = blk % 5 != 0;
                c.setExtras(e); c.setKeySel(blk % 13, blk % 3); c.setKnob((float)(blk % 11) / 10.f, v);
                const int m = std::min(512, n - p); c.process(&a[(size_t)p], nullptr, &L[(size_t)p], &R[(size_t)p], m);
            }
            gCountAlloc = false;
            for (int i = 0; i < n; ++i) if (!std::isfinite(L[(size_t)i]) || !std::isfinite(R[(size_t)i])) bad = true;
            std::printf("[3c] %s réglages extrêmes : %s, %ld allocation(s) dans process()\n", v.name, bad ? "NaN/inf !" : "aucun NaN", gAllocs);
            CHECK(!bad, "NaN/inf (%s)", v.name);
            CHECK(gAllocs == 0, "allocation en temps réel (%s) : %ld", v.name, gAllocs);
            // latence mesurée avec le volet à fond (ton, largeur, drive, sortie)
            Chain d; d.prepare(fs, 512); Extras e; e.toneDb = 6.f; e.special = 1.f; e.outDb = -6.f; e.autoIn = true; d.setExtras(e); d.setKnob(0.5f, v);
            std::vector<float> in(30000, 0.f), l2(in.size()), r2(in.size()); in[4000] = 0.01f;
            for (size_t i = 0; i < in.size(); i += 500) { d.setKnob(0.5f, v); d.process(&in[i], nullptr, &l2[i], &r2[i], 500); }
            size_t pk = 0; for (size_t i = 0; i < l2.size(); ++i) if (std::fabs(l2[i]) > std::fabs(l2[pk])) pk = i;
            std::printf("[3d] %s volet à fond : latence déclarée %d, mesurée %d\n", v.name, d.latency(), (int)pk - 4000);
            CHECK(std::abs((int)pk - 4000 - d.latency()) <= 2, "latence avec volet %s : mesurée %d, déclarée %d", v.name, (int)pk - 4000, d.latency());
        }
    }
    // 2. chaîne sur une vraie prise
    std::vector<float> in; double sr = 44100;
    const char* path = argc > 1 ? argv[1] : nullptr;
    if (!path || !readWav(path, in, sr)) { std::printf("[2] pas de WAV fourni : tests de chaîne ignorés\n"); return fails ? 1 : 0; }
    std::printf("[2] prise : %.1f s à %.0f Hz\n", (double)in.size() / sr, sr);
    double lo0, mi0, hi0, ce0; bands(in, sr, lo0, mi0, hi0, ce0);
    std::printf("    original : centroïde %.0f Hz\n", ce0);
    const float ks[3] = {0.f, 0.5f, 1.f};
    double ce[4][3], lo[4][3];
    for (int vi = 0; vi < 4; ++vi) for (int ki = 0; ki < 3; ++ki) {
        auto out = run(in, sr, vi, ks[ki]);
        bool bad = false; float pk = 0; for (float v : out) { if (!std::isfinite(v)) bad = true; pk = std::max(pk, std::fabs(v)); }
        const double dB = 20 * std::log10((rms(out) + 1e-9) / (rms(in) + 1e-9));
        double l, m, h, c; bands(out, sr, l, m, h, c); ce[vi][ki] = c; lo[vi][ki] = 10 * std::log10((l + 1e-9) / (lo0 + 1e-9));
        std::printf("    %-10s k=%.1f : niveau %+5.1f dB, pic %.2f, centroïde %5.0f Hz, grave %+5.1f dB\n", voicings()[(size_t)vi].name, ks[ki], dB, pk, c, lo[vi][ki]);
        CHECK(!bad, "NaN/inf (%s k=%.1f)", voicings()[(size_t)vi].name, ks[ki]);
        CHECK(pk <= 1.0f, "pic > 1 (%s k=%.1f)", voicings()[(size_t)vi].name, ks[ki]);
        CHECK(std::fabs(dB) < 4.5, "niveau trop différent de l'entrée : %+.1f dB (%s k=%.1f)", dB, voicings()[(size_t)vi].name, ks[ki]);
        if (ki == 1 || ki == 2) { char fn[128]; std::snprintf(fn, sizeof fn, "/tmp/onek_%s_%d.raw", voicings()[(size_t)vi].id, ki); FILE* f = std::fopen(fn, "wb"); if (f) { std::fwrite(out.data(), 4, out.size(), f); std::fclose(f); } }
    }
    CHECK(ce[2][2] > ce[2][0] * 1.08, "Poupée devrait s'éclaircir quand on monte le potard (%.0f -> %.0f Hz)", ce[2][0], ce[2][2]);
    CHECK(ce[0][2] < ce[0][1] * 0.97 || ce[0][2] < ce[0][0], "Grain Nord devrait s'assombrir en fin de course (%.0f -> %.0f Hz)", ce[0][1], ce[0][2]);
    CHECK(lo[1][2] > lo[1][0] + 0.8, "Gorge devrait gagner du grave (%.1f -> %.1f dB)", lo[1][0], lo[1][2]);
    std::printf(fails ? "\n%d ECHEC(S)\n" : "\nTESTS UN-POTARD OK\n", fails);
    return fails ? 1 : 0;
}
