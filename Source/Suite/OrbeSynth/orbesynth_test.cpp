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


    // 7. lot E : anti-repliement (BLEP à table 16 points), repliement par onde sans filtre, pire composante hors harmoniques
    {
        double worstOld = -400, worstNew = -400, worstTri = -400;
        for (int w : {Saw, Square, Tri}) for (int note : {69, 81, 93, 100}) for (int aa : {0, 1}) {
            Engine e; e.prepare(SR);
            Params p; p.aa = aa; p.w1 = w; p.mix = 0; p.uni = 1; p.det = 0; p.cut = 1; p.famt = 0; p.res = 0; p.dly = 0; p.rev = 0; p.cho = 0; p.drive = 0; p.a = .001f; p.s = 1; p.sub = 0;
            e.setParams(p); e.noteOn(note, 1.f);
            std::vector<float> l(65536 + 49152), r(l.size());
            for (size_t i = 0; i < l.size(); i += 256) e.render(&l[i], &r[i], 256);
            const double a = aliasDb(l, 48000, SR, 440.0 * std::pow(2.0, (note - 69) / 12.0));
            if (aa == 0) worstOld = std::max(worstOld, a); else { worstNew = std::max(worstNew, a); if (w == Tri) worstTri = std::max(worstTri, a); }
        }
        std::printf("synth anti-repliement (pire composante hors harmoniques, A4..E7) : PolyBLEP 2 points %.1f dB, BLEP 16 points %.1f dB (triangle %.1f)\n", worstOld, worstNew, worstTri);
        CHECK(worstNew < -80.0 && worstNew < worstOld - 30.0, "anti-repliement insuffisant : %.1f dB (ancien %.1f)", worstNew, worstOld);
        // niveau inchangé (RMS des deux chemins à 1 dB près)
        for (int w : {Saw, Square, Tri}) {
            double rms[2];
            for (int aa : {0, 1}) { Engine e; e.prepare(SR); Params p; p.aa = aa; p.w1 = w; p.mix = 0; p.uni = 1; p.det = 0; p.cut = 1; p.famt = 0; p.res = 0; p.dly = 0; p.rev = 0; p.cho = 0; p.s = 1; e.setParams(p); e.noteOn(60, 1.f);
                std::vector<float> l(48000), r(48000); e.render(l.data(), r.data(), 48000); double a = 0; for (int i = 24000; i < 48000; ++i) a += (double)l[(size_t)i] * l[(size_t)i]; rms[aa] = std::sqrt(a / 24000); }
            CHECK(std::fabs(20 * std::log10(rms[1] / rms[0])) < 1.0, "niveau de l'onde %d change de %.2f dB", w, 20 * std::log10(rms[1] / rms[0]));
        }
    }
    // 8. unisson : phases aléatoires. Le pic des 10 premières ms (toutes les voix en phase) tombe ; reproductible ; deux notes = deux tirages
    {
        auto first = [](int note, int rnd, std::vector<float>* keep) {
            Engine e; e.prepare(SR); Params p; p.uni = 7; p.det = 30; p.mix = 0; p.cut = 1; p.famt = 0; p.res = 0; p.a = .001f; p.s = 1; p.dly = 0; p.rev = 0; p.cho = 0; p.uniRand = rnd; e.setParams(p); e.noteOn(note, 1.f);
            std::vector<float> l(4800), r(4800); e.render(l.data(), r.data(), 4800);
            double rmsLate = 0, pk = 0; for (int i = 0; i < 4800; ++i) pk = std::max(pk, (double)std::fabs(l[(size_t)i]));
            { Engine e2; e2.prepare(SR); e2.setParams(p); e2.noteOn(note, 1.f); std::vector<float> a(48000), b(48000); e2.render(a.data(), b.data(), 48000); for (int i = 24000; i < 48000; ++i) rmsLate += (double)a[(size_t)i] * a[(size_t)i]; rmsLate = std::sqrt(rmsLate / 24000); }
            if (keep) *keep = l;
            return pk / rmsLate; };
        std::vector<float> a, b, c;
        const double c0 = first(57, 0, nullptr), c1 = first(57, 1, &a);
        first(57, 1, &b); first(60, 1, &c);
        std::printf("synth unisson 7 voix : facteur de crete des 100 premieres ms, phase 0 : %.2f, phases aleatoires : %.2f\n", c0, c1);
        CHECK(c1 < c0 - 0.3, "unisson aleatoire : crete %.2f contre %.2f", c1, c0);
        CHECK(a == b, "unisson aleatoire non reproductible");
        CHECK(a != c, "deux notes ont le meme tirage");
    }
    // 9. mono / legato / glisse : legato sans relance de l'enveloppe, glisse en 100 ms de A4 a A5 (passages par zero), retour a la note tenue
    {
        auto freqAt = [](const std::vector<float>& y, int i0, int n) { int z = 0; for (int i = i0 + 1; i < i0 + n; ++i) z += (y[(size_t)i - 1] <= 0.f && y[(size_t)i] > 0.f); return z * SR / n; };
        Engine e; e.prepare(SR);
        Params p; p.w1 = Sine; p.mix = 0; p.uni = 1; p.cut = 1; p.famt = 0; p.res = 0; p.a = .001f; p.s = 1; p.dly = 0; p.rev = 0; p.cho = 0; p.vol = 1; p.mode = 2; p.glide = .1f; e.setParams(p);
        std::vector<float> l, r, keepL;
        auto adv = [&](int n) { std::vector<float> a((size_t)n), b((size_t)n); e.render(a.data(), b.data(), n); keepL.insert(keepL.end(), a.begin(), a.end()); };
        e.noteOn(69, 1.f); adv(4800);
        const int voicesBefore = e.activeVoices(); const size_t t0 = keepL.size();
        e.noteOn(81, 1.f); adv(9600);
        CHECK(e.activeVoices() == voicesBefore && voicesBefore == 1, "legato : %d voix (attendu 1)", e.activeVoices());
        const double f0 = freqAt(keepL, 2400, 2400), fEnd = freqAt(keepL, (int)t0 + 6000, 2400);
        const double fHalf = freqAt(keepL, (int)t0 + 2400 - 480, 960);   // 40..60 ms : vers le milieu de la course (exponentielle en hauteur)
        std::printf("synth glide : A4 %.0f Hz -> mi-course %.0f Hz -> A5 %.0f Hz\n", f0, fHalf, fEnd);
        CHECK(std::fabs(f0 - 440) < 25 && std::fabs(fEnd - 880) < 25, "glide : %.1f / %.1f Hz", f0, fEnd);
        CHECK(fHalf > 520 && fHalf < 760, "glide : pas de course progressive (%.0f Hz a mi-course)", fHalf);
        // relache de la note du haut puis retour a la note tenue : legato avec pile
        e.noteOn(64, 1.f); adv(9600); e.noteOff(64); adv(9600);
        const double fBack = freqAt(keepL, (int)keepL.size() - 2400, 2400);
        CHECK(std::fabs(fBack - 880) < 25, "pile mono : retour a %.0f Hz (attendu 880)", fBack);
        e.noteOff(81); e.noteOff(69); Stat t; run(e, 3.0, t);
        CHECK(e.activeVoices() == 0, "mono : voix residuelles");
        // mono (mode 1) relance l'enveloppe : l'attaque 200 ms repart
        Params q = p; q.mode = 1; q.a = .2f; q.glide = 0; q.s = .3f; q.d = .05f; e.setParams(q); e.noteOn(60, 1.f); std::vector<float> a((size_t)(SR * 1.0)), b(a.size()); e.render(a.data(), b.data(), (int)a.size());
        e.noteOn(62, 1.f); std::vector<float> a2((size_t)(SR * .3)), b2(a2.size()); e.render(a2.data(), b2.data(), (int)a2.size());
        double pk0 = 0, pk1 = 0, pk2 = 0; for (int i = 0; i < 480; ++i) { pk0 = std::max(pk0, (double)std::fabs(a[a.size() - 480 + (size_t)i])); pk1 = std::max(pk1, (double)std::fabs(a2[(size_t)i]));  }
        for (size_t i = 0; i < a2.size(); ++i) pk2 = std::max(pk2, (double)std::fabs(a2[i]));
        CHECK(pk1 < pk0 * 1.5f && pk2 > pk0 * 2.0, "mono : l'enveloppe ne repart pas (sustain %.3f, debut %.3f, sommet %.3f)", pk0, pk1, pk2);
        // rafale de notes en mono et en legato : borne, finie
        for (int mo : {1, 2}) { Engine g; g.prepare(SR); Params z = p; z.mode = mo; z.glide = .05f; g.setParams(z); Stat st; for (int i = 0; i < 200; ++i) { g.noteOn(40 + (i * 7) % 30, .8f); if (i % 3 == 0) g.noteOff(40 + ((i - 3) * 7) % 30); run(g, .01, st); } g.allNotesOff(); run(g, 3.0, st); CHECK(st.finite && st.peak < 1.f && g.activeVoices() == 0, "rafale mono %d : pic %.3f, %d voix", mo, st.peak, g.activeVoices()); }
    }
    // 10. LFO : trémolo (profondeur 1 -> modulation d'amplitude profonde à la fréquence réglée), vibrato (déviation de hauteur), coupure (brillance varie)
    {
        auto run1 = [](Params p, int note, double sec) { Engine e; e.prepare(SR); e.setParams(p); e.noteOn(note, 1.f); std::vector<float> l((size_t)(SR * sec)), r(l.size()); e.render(l.data(), r.data(), (int)l.size()); return l; };
        Params p; p.w1 = Sine; p.mix = 0; p.uni = 1; p.cut = 1; p.famt = 0; p.res = 0; p.a = .001f; p.s = 1; p.dly = 0; p.rev = 0; p.cho = 0; p.vol = 1;
        Params t = p; t.lfoDest[0] = 3; t.lfoDepth[0] = 1.f; t.lfoRate[0] = 5.f; t.lfoShape[0] = 0;
        auto y = run1(t, 69, 2.0);
        double mn = 9, mx = 0; for (int w = 0; w < 190; ++w) { double pk = 0; for (int i = 0; i < 480; ++i) pk = std::max(pk, (double)std::fabs(y[(size_t)(24000 + w * 480 + i)])); mn = std::min(mn, pk); mx = std::max(mx, pk); }
        CHECK(mn < mx * .15, "tremolo : creux %.3f / crete %.3f", mn, mx);
        Params v = p; v.lfoDest[0] = 2; v.lfoDepth[0] = 1.f; v.lfoRate[0] = 4.f;
        auto yv = run1(v, 69, 2.0);
        double fmin = 1e9, fmax = 0; for (int w = 0; w < 60; ++w) { int z = 0; const int i0 = 24000 + w * 400; for (int i = i0 + 1; i < i0 + 400; ++i) z += (yv[(size_t)i - 1] <= 0.f && yv[(size_t)i] > 0.f); const double f = z * SR / 400.0; fmin = std::min(fmin, f); fmax = std::max(fmax, f); }
        std::printf("synth LFO vibrato (profondeur 1, +-2 dt) : %.0f..%.0f Hz\n", fmin, fmax);
        CHECK(fmax > 470 && fmin < 410, "vibrato : %.0f..%.0f Hz (attendu ~391..494)", fmin, fmax);
        Params c = p; c.w1 = Saw; c.cut = .5f; c.lfoDest[1] = 1; c.lfoDepth[1] = 1.f; c.lfoRate[1] = 3.f; c.lfoShape[1] = 1;
        auto yc = run1(c, 48, 2.0);
        double hmin = 1e9, hmax = 0; for (int w = 0; w < 60; ++w) { double dd = 0, tt = 0; const int i0 = 24000 + w * 400; for (int i = i0 + 1; i < i0 + 400; ++i) { const double d = yc[(size_t)i] - yc[(size_t)i - 1]; dd += d * d; tt += (double)yc[(size_t)i] * yc[(size_t)i]; } const double ratio = dd / (tt + 1e-12); hmin = std::min(hmin, ratio); hmax = std::max(hmax, ratio); }
        CHECK(hmax > hmin * 2.0, "LFO de coupure : brillance constante (%.4f..%.4f)", hmin, hmax);
        // extrêmes : tous les LFO à fond, toutes cibles : fini et borné
        for (int dest = 1; dest <= 5; ++dest) { Params x = p; x.f2mode = 1; x.res = 1; x.f2res = 1; x.lfoDest[0] = x.lfoDest[1] = dest; x.lfoDepth[0] = x.lfoDepth[1] = 1.f; x.lfoRate[0] = 30.f; x.lfoRate[1] = .05f; x.lfoShape[0] = 3; x.lfoShape[1] = 4; x.uni = 7; x.drive = 1;
            Engine g; g.prepare(SR); g.setParams(x); Stat st; for (int n : {36, 48, 60, 72}) g.noteOn(n, 1.f); run(g, 1.5, st); CHECK(st.finite && st.peak < 1.f, "LFO cible %d : pic %.3f", dest, st.peak); }
    }
    // 11. second filtre : passe-bas, passe-haut, passe-bande 12 dB/oct sur un bruit de scie ; stable a resonance maxi
    {
        auto gain = [](int mode, float cut, double f) {   // gain (dB) d'un sinus de frequence f
            Engine e; e.prepare(SR); Params p; p.w1 = Sine; p.mix = 0; p.uni = 1; p.cut = 1; p.famt = 0; p.res = 0; p.a = .001f; p.s = 1; p.dly = 0; p.rev = 0; p.cho = 0; p.vol = 1; p.f2mode = mode; p.f2cut = cut; p.f2res = 0;
            e.setParams(p); const int note = (int)std::lround(69 + 12 * std::log2(f / 440.0)); e.noteOn(note, 1.f);
            Params q = p; q.f2mode = 0; Engine b; b.prepare(SR); b.setParams(q); b.noteOn(note, 1.f);
            std::vector<float> l(48000), r(48000), l2(48000), r2(48000); e.render(l.data(), r.data(), 48000); b.render(l2.data(), r2.data(), 48000);
            double a = 0, c = 0; for (int i = 24000; i < 48000; ++i) { a += (double)l[(size_t)i] * l[(size_t)i]; c += (double)l2[(size_t)i] * l2[(size_t)i]; } return 10 * std::log10(a / c); };
        const float c1k = std::log(1000.f / 60.f) / std::log(300.f);   // coupure ~1 kHz
        const double lpHi = gain(1, c1k, 4186.0), lpLo = gain(1, c1k, 110.0), hpLo = gain(2, c1k, 110.0), hpHi = gain(2, c1k, 3951.0), bp = gain(3, c1k, 1000.0), bpLo = gain(3, c1k, 110.0);
        std::printf("synth filtre 2 (coupure 1 kHz) : LP 4,2 kHz %.1f dB / 110 Hz %.1f dB ; HP 110 Hz %.1f dB / 3,9 kHz %.1f dB ; BP 1 kHz %.1f dB / 110 Hz %.1f dB\n", lpHi, lpLo, hpLo, hpHi, bp, bpLo);
        CHECK(lpHi < -18.0 && std::fabs(lpLo) < 1.0, "LP : %.1f / %.1f dB", lpHi, lpLo);
        CHECK(hpLo < -18.0 && std::fabs(hpHi) < 1.5, "HP : %.1f / %.1f dB", hpLo, hpHi);
        CHECK(bpLo < -9.0, "BP : %.1f dB a 110 Hz", bpLo);
        for (int mode : {1, 2, 3}) { Engine e; e.prepare(SR); Params p; p.f2mode = mode; p.f2res = 1.f; p.f2cut = .6f; p.res = 1.f; p.uni = 7; e.setParams(p); Stat st; for (int n : {36, 60, 84}) e.noteOn(n, 1.f); run(e, 1.0, st); CHECK(st.finite && st.peak < 1.f, "filtre 2 mode %d : pic %.3f", mode, st.peak); }
    }
    std::printf(fails ? "\n%d ECHEC(S)\n" : "\nTOUS LES TESTS PASSENT\n", fails);
    return fails ? 1 : 0;
}
