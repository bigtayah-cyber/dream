// Tests hors-ligne du moteur Voile. Usage : offline [entree.wav sortie.wav]
#include "../Source/Engine.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

using namespace voile;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("  ECHEC: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static float estimateHz(const std::vector<float>& x, double sr) {
    Yin y; y.init(sr);
    std::vector<float> f;
    int cnt = 0; float acc = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        acc += x[i];
        if (++cnt >= y.dec) { y.push(acc / y.dec); acc = 0; cnt = 0; }
        if (i % 256 == 255 && i > x.size() / 2) { float h = y.analyze(); if (h > 0) f.push_back(h); }
    }
    if (f.empty()) return 0;
    std::sort(f.begin(), f.end());
    return f[f.size() / 2];
}

// Ecart moyen (en demi-tons) a la note la plus proche sur les trames voisees : 0 = parfaitement juste.
static float fracDev(const std::vector<float>& x, double sr, int* voiced) {
    Yin y; y.init(sr);
    int cnt = 0, v = 0; float acc = 0, sum = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        acc += x[i];
        if (++cnt >= y.dec) { y.push(acc / y.dec); acc = 0; cnt = 0; }
        if (i % 512 == 511) {
            const float h = y.analyze();
            if (h > 0) { const float m = 69.f + 12.f * std::log2(h / 440.f); sum += std::fabs(m - std::round(m)); ++v; }
        }
    }
    if (voiced) *voiced = v;
    return v ? sum / (float)v : -1.f;
}

static std::vector<float> run(Engine& e, const std::vector<float>& in, std::vector<float>* outR = nullptr) {
    std::vector<float> L(in.size()), R(in.size());
    for (size_t i = 0; i < in.size(); i += 512) {
        int n = (int)std::min<size_t>(512, in.size() - i);
        e.process(&in[i], nullptr, &L[i], &R[i], n);
    }
    if (outR) *outR = R;
    return L;
}

struct Wav { std::vector<float> ch[2]; int n = 0, nch = 1; double sr = 48000; };

static bool readWav(const char* p, Wav& w) {
    FILE* f = std::fopen(p, "rb"); if (!f) return false;
    std::vector<uint8_t> b; uint8_t t[65536]; size_t r;
    while ((r = std::fread(t, 1, sizeof t, f)) > 0) b.insert(b.end(), t, t + r);
    std::fclose(f);
    if (b.size() < 44 || std::memcmp(&b[0], "RIFF", 4) || std::memcmp(&b[8], "WAVE", 4)) return false;
    int fmt = 0, bits = 0; size_t pos = 12, dpos = 0, dlen = 0;
    while (pos + 8 <= b.size()) {
        uint32_t sz; std::memcpy(&sz, &b[pos + 4], 4);
        if (!std::memcmp(&b[pos], "fmt ", 4)) {
            uint16_t a, c, bp; uint32_t s;
            std::memcpy(&a, &b[pos + 8], 2); std::memcpy(&c, &b[pos + 10], 2);
            std::memcpy(&s, &b[pos + 12], 4); std::memcpy(&bp, &b[pos + 22], 2);
            fmt = a; w.nch = c; w.sr = s; bits = bp;
            if (fmt == 0xFFFE && sz >= 26) { uint16_t sub; std::memcpy(&sub, &b[pos + 32], 2); fmt = sub; }
        } else if (!std::memcmp(&b[pos], "data", 4)) { dpos = pos + 8; dlen = std::min<size_t>(sz, b.size() - dpos); break; }
        pos += 8 + sz + (sz & 1);
    }
    if (!dpos || w.nch < 1 || w.nch > 2) return false;
    const int bps = bits / 8;
    w.n = (int)(dlen / (size_t)(bps * w.nch));
    for (int c = 0; c < w.nch; ++c) w.ch[c].resize((size_t)w.n);
    for (int i = 0; i < w.n; ++i) for (int c = 0; c < w.nch; ++c) {
        const uint8_t* q = &b[dpos + ((size_t)i * w.nch + c) * bps]; float v = 0;
        if (fmt == 3 && bits == 32) std::memcpy(&v, q, 4);
        else if (bits == 16) v = (int16_t)(q[0] | (q[1] << 8)) / 32768.f;
        else if (bits == 24) v = (int32_t)((q[0] << 8) | (q[1] << 16) | (q[2] << 24)) / 2147483648.f;
        else if (bits == 32) { int32_t k; std::memcpy(&k, q, 4); v = k / 2147483648.f; }
        w.ch[c][(size_t)i] = v;
    }
    return true;
}

static void writeWav(const char* p, const std::vector<float>& L, const std::vector<float>& R, double sr) {
    FILE* f = std::fopen(p, "wb"); if (!f) return;
    uint32_t n = (uint32_t)L.size(), dl = n * 4, sz = 36 + dl, sri = (uint32_t)sr, br = sri * 4, k16 = 16;
    uint16_t pcm = 1, ch = 2, ba = 4, bp = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&sz, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&k16, 4, 1, f); std::fwrite(&pcm, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&sri, 4, 1, f);
    std::fwrite(&br, 4, 1, f); std::fwrite(&ba, 2, 1, f); std::fwrite(&bp, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&dl, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) for (const float v : {L[i], R[i]}) {
        int16_t s = (int16_t)std::lround(std::max(-1.f, std::min(1.f, v)) * 32767.f); std::fwrite(&s, 2, 1, f);
    }
    std::fclose(f);
}


// ---- chantier 12 : mesure de repliement hors-ligne (FFT de Blackman-Harris, 65536 points) ----
static void fftD(std::vector<double>& re, std::vector<double>& im) {
    const size_t n = re.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * 3.14159265358979323846 / (double)len;
        for (size_t i = 0; i < n; i += len) for (size_t k = 0; k < len / 2; ++k) {
            const double wr = std::cos(ang * (double)k), wi = std::sin(ang * (double)k);
            const size_t a = i + k, b = a + len / 2;
            const double tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
            re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
        }
    }
}
// Pire composante (dB re fondamental) hors des harmoniques de f0 (garde 60 Hz) entre 30 Hz et fmax : c'est le repliement.
static double aliasDb(const std::vector<float>& y, double sr, double f0, double fmax = 20000.0) {
    const size_t N = 65536; std::vector<double> re(N), im(N, 0.0);
    const size_t off = y.size() - N;
    for (size_t i = 0; i < N; ++i) { const double w = 0.35875 - 0.48829 * std::cos(2 * 3.14159265358979 * (double)i / (double)(N - 1)) + 0.14128 * std::cos(4 * 3.14159265358979 * (double)i / (double)(N - 1)) - 0.01168 * std::cos(6 * 3.14159265358979 * (double)i / (double)(N - 1)); re[i] = w * y[off + i]; }
    fftD(re, im);
    double pf = 0, worst = 0; const double bw = sr / (double)N;
    for (size_t k = 1; k < N / 2; ++k) {
        const double f = (double)k * bw, p = re[k] * re[k] + im[k] * im[k];
        const int h = (int)std::lround(f / f0);
        if (h >= 1 && std::fabs(f - h * f0) < 60.0) { if (h == 1) pf += p; continue; }
        if (f < 30.0 || f > fmax) continue;
        worst = std::max(worst, p);
    }
    return 10.0 * std::log10(worst / pf + 1e-30);
}

int main(int argc, char** argv) {
    const double sr = 48000;
    Params off; off.mordant = 0; off.brume = 0; off.halo = 0; off.mix = 1;

    std::printf("[1] Correction de hauteur : sinus 225 Hz -> A3 (220 Hz)\n");
    {
        Engine e; Params p = off; p.tension = 1.f; p.key = 0; p.scale = 0; e.set(p); e.prepare(sr); e.set(p);
        std::vector<float> in((size_t)(sr * 3));
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.3f * std::sin(2 * kPi * 225.f * (float)i / (float)sr);
        const float hz = estimateHz(run(e, in), sr);
        std::printf("  sortie estimee : %.2f Hz\n", hz);
        CHECK(std::fabs(hz - 220.f) < 2.f, "attendu ~220 Hz, obtenu %.2f", hz);
    }
    std::printf("[2] Gamme : 233 Hz (A#3) en C majeur -> A3 ou B3 (pas de note hors gamme)\n");
    {
        Engine e; Params p = off; p.tension = 1.f; p.key = 0; p.scale = 1; e.set(p); e.prepare(sr); e.set(p);
        std::vector<float> in((size_t)(sr * 3));
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.3f * std::sin(2 * kPi * 232.f * (float)i / (float)sr);
        const float hz = estimateHz(run(e, in), sr);
        const float midi = 69.f + 12.f * std::log2(hz / 440.f);
        std::printf("  sortie : %.2f Hz (midi %.2f)\n", hz, midi);
        CHECK(allowedNote((int)std::lround(midi), 0, 1) && std::fabs(midi - std::round(midi)) < 0.15f, "note hors gamme ou non snappee");
    }
    std::printf("[3] Tension 0 : hauteur inchangee\n");
    {
        Engine e; Params p = off; p.tension = 0.f; e.set(p); e.prepare(sr); e.set(p);
        std::vector<float> in((size_t)(sr * 3));
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.3f * std::sin(2 * kPi * 225.f * (float)i / (float)sr);
        const float hz = estimateHz(run(e, in), sr);
        CHECK(std::fabs(hz - 225.f) < 2.f, "attendu ~225 Hz, obtenu %.2f", hz);
    }
    std::printf("[4] Latence declaree vs mesuree\n");
    {
        Engine e; Params p = off; p.tension = 0.f; e.set(p); e.prepare(sr); e.set(p);
        std::vector<float> in((size_t)sr, 0.f); in[2000] = 0.5f;
        auto o = run(e, in);
        size_t am = 0; for (size_t i = 0; i < o.size(); ++i) if (std::fabs(o[i]) > std::fabs(o[am])) am = i;
        std::printf("  declaree %d, mesuree %d\n", e.latency(), (int)am - 2000);
        CHECK(std::abs((int)am - 2000 - e.latency()) <= 1, "latence incorrecte");
    }
    std::printf("[5] Stabilite : bruit + tous parametres au maximum\n");
    {
        Engine e; Params p; p.tension = 1; p.mordant = 1; p.brume = 1; p.halo = 1; p.mix = 1; p.outDb = 12; e.set(p); e.prepare(sr); e.set(p);
        std::mt19937 g(1); std::uniform_real_distribution<float> u(-1.f, 1.f);
        std::vector<float> in((size_t)(sr * 4)); for (auto& v : in) v = 0.8f * u(g);
        std::vector<float> R; auto L = run(e, in, &R);
        float pk = 0; bool bad = false;
        for (size_t i = 0; i < L.size(); ++i) { if (!std::isfinite(L[i]) || !std::isfinite(R[i])) bad = true; pk = std::max({pk, std::fabs(L[i]), std::fabs(R[i])}); }
        std::printf("  crete max %.3f\n", pk);
        CHECK(!bad, "NaN/inf detectes"); CHECK(pk <= 1.001f, "depassement 0 dBFS : %.3f", pk);
    }
    std::printf("[6] Queue : le signal revient au silence apres arret (reverb/delay stables)\n");
    {
        Engine e; Params p; p.brume = 1; p.mix = 1; e.set(p); e.prepare(sr); e.set(p);
        std::vector<float> in((size_t)(sr * 12), 0.f);
        for (size_t i = 0; i < (size_t)(sr * 0.5); ++i) in[i] = 0.5f * std::sin(2 * kPi * 330.f * (float)i / (float)sr);
        std::vector<float> R; auto L = run(e, in, &R);
        float tail = 0; for (size_t i = L.size() - (size_t)sr; i < L.size(); ++i) tail = std::max(tail, std::fabs(L[i]));
        std::printf("  amplitude de la derniere seconde : %.5f\n", tail);
        CHECK(tail < 0.01f, "la queue ne decroit pas (%.5f)", tail);
    }
    std::printf("[6b] Suréchantillonneur : latence mesurée = déclarée, gain plat, coupure\n");
    for (int st = 1; st <= 3; ++st) {
        Oversampler o; o.init(st);
        std::vector<float> h(4000, 0.f);
        for (size_t n = 0; n < h.size(); ++n) h[n] = o.process(n == 0 ? 1.f : 0.f, [](float v) { return v; });
        size_t pk = 0; for (size_t n = 0; n < h.size(); ++n) if (std::fabs(h[n]) > std::fabs(h[pk])) pk = n;
        auto gain = [&](double f) { double re = 0, im = 0; for (size_t n = 0; n < h.size(); ++n) { re += h[n] * std::cos(2 * 3.14159265358979 * f * (double)n); im -= h[n] * std::sin(2 * 3.14159265358979 * f * (double)n); } return 20.0 * std::log10(std::hypot(re, im)); };
        std::printf("  x%d : latence %d (pic mesuré %zu), gain 0,1 fs %.3f dB, 0,4 fs %.3f dB\n", 1 << st, o.latency(), pk, gain(0.1), gain(0.4));
        CHECK((int)pk == o.latency(), "latence du suréchantillonneur %d != pic %zu", o.latency(), pk);
        CHECK(std::fabs(gain(0.1)) < 0.01 && std::fabs(gain(0.4)) < 0.01, "gain non plat");
    }
    std::printf("[6c] Repliement : sinus 5 kHz poussé à fond dans la saturation (critère < -70 dB)\n");
    for (double fs : {44100.0, 48000.0, 96000.0}) {
        const double f0 = 5000.0;
        Oversampler o; o.init(Oversampler::stagesFor(fs, 3));
        std::vector<float> yo(200000), y1(200000);
        for (size_t n = 0; n < yo.size(); ++n) {
            const float x = (float)(0.99 * std::sin(2.0 * 3.14159265358979 * f0 * (double)n / fs));   // phase en double : pas de bruit d'entrée
            auto sat = [](float v) { return std::tanh(10.f * v); };   // drive maximal (1 + 9 m, m = 1)
            yo[n] = o.process(x, sat); y1[n] = sat(x);
        }
        const double a1 = aliasDb(y1, fs, f0), ao = aliasDb(yo, fs, f0);
        std::printf("  %.0f Hz : tanh x10 sans suréchantillonnage %.1f dB -> avec (latence %d) %.1f dB\n", fs, a1, o.latency(), ao);
        CHECK(ao < -70.0, "repliement de la saturation %.1f dB à %.0f Hz", ao, fs);
    }
    std::printf("[6d] Repliement dans le moteur Voile (mordant 0,45 = drive x5, 12 bits), sinus 5 kHz\n");
    for (double fs : {44100.0, 48000.0, 96000.0}) {
        const double f0 = 5000.0;
        Engine e; Params p = off; p.tension = 0.f; p.mordant = 0.45f; e.set(p); e.prepare(fs); e.set(p);
        std::vector<float> in(240000); for (size_t i = 0; i < in.size(); ++i) in[i] = (float)(0.9 * std::sin(2.0 * 3.14159265358979 * f0 * (double)i / fs));
        const double a = aliasDb(run(e, in), fs, f0);
        std::printf("  %.0f Hz : repliement %.1f dB (latence déclarée %d)\n", fs, a, e.latency());
        CHECK(a < -70.0, "repliement du moteur %.1f dB à %.0f Hz", a, fs);
    }
    std::printf("[6e] Décaleur : à décalage nul la sortie est le sec retardé de win/2, sans peigne, même après un décalage\n");
    {
        Shifter sh; sh.init(1536);
        std::mt19937 g(3); std::uniform_real_distribution<float> u(-1.f, 1.f);
        std::vector<float> x(40000); for (auto& v : x) v = u(g);
        double err = 0, ref = 0; bool moved = false;
        for (size_t n = 0; n < x.size(); ++n) {
            const float ratio = n < 20000 ? 1.0594631f : 1.f;   // un demi-ton puis rien
            const float y = sh.process(x[n], ratio);
            if (n >= 20000 + 1600) { const float d = x[n - 768]; err += (double)(y - d) * (y - d); ref += (double)d * d; moved = true; }
        }
        const double db = 10.0 * std::log10(err / ref + 1e-30);
        std::printf("  erreur par rapport au sec retardé : %.1f dB\n", db);
        CHECK(moved && db < -100.0, "décalage nul : peigne résiduel %.1f dB", db);
    }
    // Voix synthetique : 6 notes avec vibrato, harmoniques decroissants, pauses courtes.
    auto voice = [&](float peakDb, double dur) {
        std::vector<float> x((size_t)(sr * dur));
        const float notes[6] = {196.f, 220.f, 247.f, 262.f, 220.f, 196.f};
        float ph = 0.f;
        for (size_t i = 0; i < x.size(); ++i) {
            const double t = (double)i / sr; const int k = std::min(5, (int)(t / (dur / 6.0))); const double tk = std::fmod(t, dur / 6.0) / (dur / 6.0);
            const float env = tk < 0.9 ? std::min(1.f, (float)(tk * 12.0)) : 0.f;
            const float f = notes[k] * (1.f + 0.012f * std::sin(2 * kPi * 5.5f * (float)t));
            ph += 2 * kPi * f / (float)sr; float v = 0; for (int h = 1; h <= 8; ++h) v += std::sin(h * ph) / (float)h;
            x[i] = v * env;
        }
        float pk = 0; for (float v : x) pk = std::max(pk, std::fabs(v));
        const float g = std::pow(10.f, peakDb / 20.f) / pk; for (auto& v : x) v *= g;
        return x;
    };
    auto rmsDb = [](const std::vector<float>& a, const std::vector<float>& b, size_t from) {
        double s = 0; size_t n = 0; for (size_t i = from; i < a.size(); ++i) { s += 0.5 * ((double)a[i] * a[i] + (double)b[i] * b[i]); ++n; }
        return 10.0 * std::log10(s / (double)n + 1e-20);
    };
    std::printf("[8] Mordant compense en niveau : traite - sec, voix seule (mordComp = false puis true)\n");
    {
        double worstOld = 0, worstNew = 0;
        for (float dbPk : {-6.f, -18.f}) for (float m : {0.3f, 0.6f, 1.f}) {
            const auto in = voice(dbPk, 6.0);
            double dd[2];
            for (int c = 0; c < 2; ++c) {
                Engine e; Params p; p.tension = 0.f; p.mordant = m; p.brume = 0; p.halo = 0; p.mix = 1; p.mordComp = c == 1; e.set(p); e.prepare(sr); e.set(p);
                std::vector<float> R; auto L = run(e, in, &R);
                std::vector<float> ds(in.size()); for (size_t i = 0; i < in.size(); ++i) ds[i] = i >= (size_t)e.latency() ? in[i - (size_t)e.latency()] : 0.f;
                dd[c] = rmsDb(L, R, (size_t)(sr * 1.5)) - rmsDb(ds, ds, (size_t)(sr * 1.5));
            }
            std::printf("  pic %+.0f dBFS, mordant %.1f : ancien %+.1f dB, compense %+.1f dB\n", dbPk, m, dd[0], dd[1]);
            worstOld = std::max(worstOld, std::fabs(dd[0])); worstNew = std::max(worstNew, std::fabs(dd[1]));
        }
        CHECK(worstOld > 4.0, "l'ancien rendu aurait du etre nettement plus fort (%.1f dB)", worstOld);
        CHECK(worstNew < 2.0, "ecart de niveau traite/sec trop grand avec la compensation (%.1f dB)", worstNew);
    }
    std::printf("[9] Halo stereo : correlation G/D de la part ajoutee, et sortie finie\n");
    {
        const auto in = voice(-6.f, 6.0);
        double corr[2], full[2];
        for (int c = 0; c < 2; ++c) {
            Engine e; Params p; p.tension = 0.f; p.mordant = 0; p.brume = 0; p.halo = 1; p.mix = 1; p.haloStereo = c == 1; e.set(p); e.prepare(sr); e.set(p);
            std::vector<float> R; auto L = run(e, in, &R);
            double lr = 0, ll = 0, rr = 0, flr = 0, fll = 0, frr = 0;
            for (size_t i = (size_t)(sr * 1.5); i < L.size(); ++i) {
                const double m = 0.5 * ((double)L[i] + R[i]);   // centre ~ voix principale : on retire la part commune
                const double a = L[i] - m, b = R[i] - m; lr += a * b; ll += a * a; rr += b * b;
                flr += (double)L[i] * R[i]; fll += (double)L[i] * L[i]; frr += (double)R[i] * R[i];
                CHECK(std::isfinite(L[i]) && std::isfinite(R[i]), "NaN halo");
            }
            // part ajoutee : (L - R) / 2 contre la somme : cos entre L-m et R-m vaut -1 par construction, on mesure plutot L/R totaux
            corr[c] = flr / std::sqrt(fll * frr + 1e-30); full[c] = 10.0 * std::log10((fll + frr) / 2.0 + 1e-20); (void)lr; (void)ll; (void)rr;
        }
        std::printf("  correlation G/D : ancien %.3f, stereo %.3f ; niveau %.1f -> %.1f dB\n", corr[0], corr[1], full[0], full[1]);
        CHECK(corr[1] < corr[0] - 0.02, "le Halo stereo n'est pas plus large que l'ancien");
        CHECK(corr[1] < 0.985, "le Halo stereo reste presque mono (%.3f)", corr[1]);
        CHECK(std::fabs(full[1] - full[0]) < 3.0, "le niveau change trop avec le Halo stereo (%.1f dB)", full[1] - full[0]);
    }
    if (argc >= 3) {
        Wav w;
        if (!readWav(argv[1], w)) { std::printf("Lecture impossible : %s\n", argv[1]); return 2; }
        std::printf("[7] Fichier : %s (%d Hz, %d canal, %.1f s)\n", argv[1], (int)w.sr, w.nch, w.n / w.sr);
        Engine e; Params p; p.tension = 0.8f; p.mordant = 0.35f; p.brume = 0.4f; p.halo = 0.35f; p.key = 0; p.scale = 0;
        e.set(p); e.prepare(w.sr); e.set(p);
        std::vector<float> L((size_t)w.n), R((size_t)w.n);
        for (int i = 0; i < w.n; i += 512) {
            int n = std::min(512, w.n - i);
            e.process(&w.ch[0][(size_t)i], w.nch > 1 ? &w.ch[1][(size_t)i] : nullptr, &L[(size_t)i], &R[(size_t)i], n);
        }
        float pi = 0, po = 0; bool bad = false;
        for (int i = 0; i < w.n; ++i) { pi = std::max(pi, std::fabs(w.ch[0][(size_t)i])); po = std::max({po, std::fabs(L[(size_t)i]), std::fabs(R[(size_t)i])}); if (!std::isfinite(L[(size_t)i])) bad = true; }
        std::printf("  crete entree %.3f -> sortie %.3f, finis : %s\n", pi, po, bad ? "NON" : "oui");
        CHECK(!bad, "NaN dans le rendu du fichier");
        {   // mesure de justesse : moteur seul (tension max, sans effets) sur la voix reelle
            Engine e2; Params q; q.tension = 1.f; q.mordant = 0; q.brume = 0; q.halo = 0; q.scale = 0; e2.set(q); e2.prepare(w.sr); e2.set(q);
            const std::vector<float> mono = w.ch[0];
            int v0 = 0, v1 = 0;
            const float d0 = fracDev(mono, w.sr, &v0), d1 = fracDev(run(e2, mono), w.sr, &v1);
            std::printf("  justesse (ecart moyen a la note) : entree %.3f -> sortie %.3f demi-ton (%d / %d trames voisees)\n", d0, d1, v0, v1);
            CHECK(v0 > 20, "tres peu de voix detectee dans ce fichier");
            CHECK(v0 <= 20 || d1 < d0 * 0.6f, "la correction n'ameliore pas la justesse");
        }
        writeWav(argv[2], L, R, w.sr);
        std::printf("  ecrit : %s\n", argv[2]);
    }
    std::printf(fails ? "\n%d ECHEC(S)\n" : "\nTOUS LES TESTS PASSENT\n", fails);
    return fails ? 1 : 0;
}
