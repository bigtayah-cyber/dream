// Test console du moteur Orbe Synth (sans JUCE) : pas de NaN, pic borné, silence après relâche, polyphonie plafonnée, reproductibilité.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <algorithm>
#include "OrbeSynthEngine.h"
using namespace osyn;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("  ECHEC: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const double SR = 48000.0;
struct Stat { bool finite = true; float peak = 0.f; };
static void feed(Stat& s, const float* l, const float* r, int n) {
    for (int i = 0; i < n; ++i) { if (!std::isfinite(l[i]) || !std::isfinite(r[i])) s.finite = false; s.peak = std::max({s.peak, std::fabs(l[i]), std::fabs(r[i])}); }
}
static void run(Engine& e, double sec, Stat& s, std::vector<float>* keep = nullptr, int blk = 480) {
    std::vector<float> l((size_t)blk), r((size_t)blk);
    for (int done = 0, tot = (int)(sec * SR); done < tot; done += blk) {
        e.render(l.data(), r.data(), blk);
        feed(s, l.data(), r.data(), blk);
        if (keep) { keep->insert(keep->end(), l.begin(), l.end()); keep->insert(keep->end(), r.begin(), r.end()); }
    }
}

// Repliement : FFT Blackman-Harris 65536 points, pire composante hors harmoniques (30 Hz a 20 kHz), dB re fondamental.
static void fftD(std::vector<double>& re, std::vector<double>& im) {
    const size_t n = re.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2 * 3.14159265358979 / (double)len, wr = std::cos(ang), wi = std::sin(ang);
        for (size_t i = 0; i < n; i += len) { double cr = 1, ci = 0; for (size_t k = 0; k < len / 2; ++k) {
            const double ur = re[i + k], ui = im[i + k], vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci, vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
            re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
            const double t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t; } }
    }
}
static double aliasDb(const std::vector<float>& y, size_t start, double sr, double f0) {
    const size_t N = 65536; std::vector<double> re(N), im(N, 0.0);
    for (size_t i = 0; i < N; ++i) { const double t = (double)i / (double)(N - 1), w = 0.35875 - 0.48829 * std::cos(2 * 3.14159265358979 * t) + 0.14128 * std::cos(4 * 3.14159265358979 * t) - 0.01168 * std::cos(6 * 3.14159265358979 * t); re[i] = w * y[start + i]; }
    fftD(re, im);
    const double guard = std::min(60.0, 0.15 * f0), bw = sr / (double)N; double pf = 0, worst = 0;
    for (size_t k = 1; k < N / 2; ++k) {
        const double f = (double)k * bw, p = re[k] * re[k] + im[k] * im[k]; const int h = (int)std::lround(f / f0);
        if (h >= 1 && std::fabs(f - h * f0) < guard) { if (h == 1) pf += p; continue; }
        if (f < 30.0 || f > 20000.0) continue;
        worst = std::max(worst, p);
    }
    return 10.0 * std::log10(worst / pf + 1e-30);
}

int main(int argc, char**) {
    // 0. calage de la reverb : gain de puissance d'un bruit blanc mono (cible : convolution normalisée de prototype ~0,2)
    if (argc > 1) {
        voile::Reverb rv; rv.init(SR); rv.setDecay(2.f);
        unsigned q = 1; double pi = 0, po = 0;
        for (int i = 0; i < 48000 * 8; ++i) { q = q * 1664525u + 1013904223u; const float x = ((q >> 8) & 0xFFFF) / 32768.f - 1.f; float a, b; rv.process(x, .4f, a, b); if (i > 48000) { pi += x * x; po += .5 * (a * a + b * b); } }
        std::printf("gain reverb (puissance) = %.4f\n", po / pi);
    }

    // 1. tous les presets : finis, bornés, audibles ; 2. silence après relâche
    for (size_t pi = 0; pi < presets().size(); ++pi) {
        const auto& pr = presets()[pi];
        for (int sr : {44100, 48000, 96000}) {
            Engine e; e.prepare(sr); e.setParams(pr.p);
            Stat st;
            for (int n : {48, 52, 55, 60, 64, 67}) e.noteOn(n, .8f);
            std::vector<float> buf((size_t)sr), buf2((size_t)sr);
            for (int k = 0; k < 2 * sr / 512; ++k) { e.render(buf.data(), buf2.data(), 512); feed(st, buf.data(), buf2.data(), 512); }
            CHECK(st.finite, "%s @%d : NaN/inf", pr.name, sr);
            CHECK(st.peak > .02f, "%s @%d : silence a l'attaque (pic %.4f)", pr.name, sr, st.peak);
            CHECK(st.peak < 1.f, "%s @%d : pic %.3f >= 1", pr.name, sr, st.peak);
            for (int n : {48, 52, 55, 60, 64, 67}) e.noteOff(n);
            Stat tail; run(e, 1.0, tail);  // laisse finir la relâche des plus longues
            Stat late; for (int k = 0; k < 14; ++k) { Stat t; run(e, 1.0, t); if (k == 13) late = t; }
            CHECK(late.finite && late.peak < 1e-5f, "%s @%d : pas de silence apres relache (pic %.2e)", pr.name, sr, late.peak);
            CHECK(e.activeVoices() == 0, "%s @%d : %d voix encore actives", pr.name, sr, e.activeVoices());
        }
    }

    // 3. pire cas : 40 notes fortes, tout à fond -> pic borné, fini
    {
        Engine e; e.prepare(SR);
        Params p; p.uni = 7; p.det = 50; p.mix = 1; p.fm = 1; p.sub = 1; p.res = 1; p.famt = 1; p.cut = 1; p.drive = 1; p.cho = 1; p.dly = 1; p.rev = 1; p.vol = 1; p.s = 1; p.a = .001f;
        e.setParams(p);
        Stat st; int maxV = 0;
        for (int i = 0; i < 40; ++i) { e.noteOn(24 + i * 2, 1.f); maxV = std::max(maxV, e.activeVoices()); run(e, .05, st); maxV = std::max(maxV, e.activeVoices()); }
        run(e, 1.0, st);
        CHECK(st.finite, "pire cas : NaN/inf");
        CHECK(st.peak < 1.f && st.peak > .3f, "pire cas : pic %.3f", st.peak);
        CHECK(maxV <= kMaxVoices, "polyphonie %d > %d", maxV, kMaxVoices);
        CHECK(e.activeVoices() == kMaxVoices, "polyphonie finale %d (attendu %d)", e.activeVoices(), kMaxVoices);
        e.allNotesOff(); Stat t; run(e, 12.0, t);
        CHECK(e.activeVoices() == 0, "voix residuelles apres allNotesOff");
        std::printf("pire cas : pic %.3f, voix max %d\n", st.peak, maxV);
    }

    // 4. reproductibilité : deux moteurs, mêmes événements -> mêmes bits ; réutilisation après reset() idem
    {
        auto go = [](Engine& e, std::vector<float>& out) {
            e.setParams(presets()[5].p); e.setBend(.3f); e.setMod(.4f);
            Stat s; e.noteOn(60, .9f); run(e, .2, s, &out); e.noteOn(67, .5f); run(e, .3, s, &out); e.noteOff(60); run(e, .5, s, &out);
        };
        Engine a, b; a.prepare(SR); b.prepare(SR);
        std::vector<float> oa, ob, oc; go(a, oa); go(b, ob);
        CHECK(oa == ob, "deux moteurs identiques divergent");
        a.reset(); go(a, oc);
        CHECK(oa == oc, "reset() ne restaure pas un etat reproductible");
    }

    // 5. justesse : sinus pur, A4 -> 440 Hz (passages par zéro) ; molette de pitch +2 demi-tons -> x1,122
    {
        Engine e; e.prepare(SR);
        Params p; p.w1 = Sine; p.mix = 0; p.uni = 1; p.cut = 1; p.famt = 0; p.res = 0; p.a = .001f; p.s = 1; p.drive = 0; p.cho = 0; p.dly = 0; p.rev = 0; p.vol = 1;
        e.setParams(p); e.noteOn(69, 1.f);
        std::vector<float> l(48000), r(48000);
        e.render(l.data(), r.data(), 4800);
        e.render(l.data(), r.data(), 48000);
        int z = 0; for (int i = 1; i < 48000; ++i) z += (l[i - 1] <= 0.f && l[i] > 0.f);
        CHECK(std::abs(z - 440) <= 1, "A4 : %d Hz au lieu de 440", z);
        e.setBend(1.f); e.render(l.data(), r.data(), 4800); e.render(l.data(), r.data(), 48000);
        z = 0; for (int i = 1; i < 48000; ++i) z += (l[i - 1] <= 0.f && l[i] > 0.f);
        CHECK(std::abs(z - 494) <= 2, "bend +2 : %d Hz au lieu de 494", z);
    }

    // 6. suréchantillonnage de la saturation : sinus 5,27 kHz à fond -> repliement < -70 dB ; latence déclarée stable et courte
    for (int sr : {44100, 48000, 96000}) for (float drv : {.6f, 1.f}) {
        Engine e; e.prepare(sr);
        CHECK(e.latency() > 0 && e.latency() < 64, "synth @%d : latence %d hors plage", sr, e.latency());
        Params p; p.w1 = Sine; p.mix = 0; p.uni = 1; p.cut = 1; p.famt = 0; p.res = 0; p.a = .001f; p.s = 1; p.drive = drv; p.cho = 0; p.dly = 0; p.rev = 0; p.vol = .8f; p.sub = 0;
        e.setParams(p); e.noteOn(112, 1.f);
        std::vector<float> l((size_t)(sr * 2.4)), r(l.size());
        e.render(l.data(), r.data(), (int)l.size());
        const double a = aliasDb(l, (size_t)(sr * 0.5), (double)sr, 440.0 * std::pow(2.0, 43.0 / 12.0));
        std::printf("synth @%d : latence %d, repliement drive %.1f 5,27 kHz = %.1f dB\n", sr, e.latency(), drv, a);
        const double th = drv < .9f ? -70.0 : -60.0;   // drive maxi = carré quasi parfait : seuil -60 dB (limiteur de sortie en ADAA)
        CHECK(a < th, "synth @%d drive %.1f : repliement %.1f dB (seuil %.0f)", sr, drv, a, th);
    }

    std::printf(fails ? "\n%d ECHEC(S)\n" : "\nTOUS LES TESTS PASSENT\n", fails);
    return fails ? 1 : 0;
}
