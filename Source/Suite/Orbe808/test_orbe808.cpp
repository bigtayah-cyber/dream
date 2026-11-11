// Test du moteur Orbe 808 sans JUCE : pas de NaN, pic borné, silence après note-off, reproductibilité, glissé legato.
#include <cmath>
#include <cstdio>
#include <vector>
#include "Orbe808Engine.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)

static o808::Params preset(int i) {
    const auto& q = o808::presets()[i];
    o808::Params p; p.len = q.len; p.punch = q.punch; p.pt = q.pt; p.drive = q.drive; p.sat = q.sat; p.tone = q.tone; p.cut = q.cut; p.click = q.click;
    return p;
}
static float peakOf(const std::vector<float>& v, size_t a, size_t b) { float m = 0; for (size_t i = a; i < b && i < v.size(); ++i) m = std::max(m, std::fabs(v[i])); return m; }
static bool finite(const std::vector<float>& v) { for (float x : v) if (!std::isfinite(x)) return false; return true; }

static std::vector<float> run(int pi, double sr, int note) {
    o808::Engine e; e.prepare(sr); e.setParams(preset(pi));
    std::vector<float> out((size_t)(sr * 2.0));
    e.noteOn(note, 1.f);
    e.render(out.data(), (int)(sr * 1.0));
    e.noteOff(note);
    e.render(out.data() + (size_t)sr, (int)(sr * 1.0));
    return out;
}


// ---- chantier 12 : repliement hors-ligne (FFT de Blackman-Harris 65536 points) ----
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
// Pire composante (dB re fondamental) hors des harmoniques de f0 (garde proportionnelle), de 30 Hz à 20 kHz.
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
// Note tenue (réglages sans clic ni harmonique ajouté), rendu sur 2,6 s ; mesure après 0,5 s. Depuis la saturation après
// l'enveloppe (vague 2B), le spectre suit le niveau : on fige l'enveloppe (len 600 s, -0,1 dB sur la fenêtre) pour ne mesurer
// que le repliement, sans l'étalement dû à l'évolution voulue des harmoniques.
static double aliasOf(double sr, int note, int sat, float drive) {
    o808::Engine e; e.prepare(sr);
    o808::Params p; p.len = 600.f; p.punch = 0; p.pt = 0.05f; p.drive = drive; p.sat = sat; p.tone = 0; p.cut = 1.f; p.click = 0; p.glide = 0.12f; p.level = 0.8f; e.setParams(p);
    e.noteOn(note, 1.f);
    std::vector<float> y((size_t)(sr * 2.6)); e.render(y.data(), (int)y.size());
    return aliasDb(y, (size_t)(sr * 0.5), sr, 440.0 * std::pow(2.0, (note - 69) / 12.0));
}

int main() {
    for (double sr : {44100.0, 48000.0, 96000.0})
        for (int pi = 0; pi < o808::kNumPresets; ++pi)
            for (int note : {24, 33, 45}) {
                auto a = run(pi, sr, note), b = run(pi, sr, note);
                CHECK(finite(a), "NaN/inf preset %d note %d sr %.0f", pi, note, sr);
                const float pk = peakOf(a, 0, a.size());
                CHECK(pk <= 1.0f, "pic %.3f > 1 (preset %d)", pk, pi);
                CHECK(pk > 0.1f, "silence au lieu d'une note (preset %d)", pi);
                CHECK(peakOf(a, (size_t)(sr * 1.5), a.size()) < 1e-4f, "bruit residuel apres note-off (preset %d, %.6f)", pi, peakOf(a, (size_t)(sr * 1.5), a.size()));
                CHECK(a == b, "non reproductible (preset %d)", pi);
            }
    {   // legato : la hauteur rejoint la nouvelle note, sans retrigger ; pas de NaN avec glisse + pitch-bend + notes rapides
        const double sr = 48000;
        o808::Engine e; e.prepare(sr); e.setParams(preset(1));
        std::vector<float> out((size_t)sr * 3);
        e.noteOn(33, 1.f); e.render(out.data(), (int)sr / 2);
        e.noteOn(45, 0.8f); e.setBend(1.f); e.render(out.data() + (size_t)sr / 2, (int)sr / 2);
        CHECK(e.currentNote() == 45, "legato : note courante %d", e.currentNote());
        e.noteOff(45); e.render(out.data() + (size_t)sr, (int)sr / 4);
        CHECK(e.currentNote() == 33, "retour a la note tenue (%d)", e.currentNote());
        e.noteOff(33); e.render(out.data() + (size_t)sr * 5 / 4, (int)sr * 3 / 4);
        for (int i = 0; i < 40; ++i) { e.noteOn(24 + i % 20, 1.f); e.render(out.data() + 1000 + i * 17, 17); if (i % 3 == 0) e.noteOff(24 + i % 20); }
        e.allOff(); e.render(out.data(), (int)sr);
        CHECK(finite(out) && peakOf(out, 0, out.size()) <= 1.f, "legato/rafale non borne");
    }
    {   // chantier 12 : saturation suréchantillonnée (x8, antidérivée) -> repliement < -70 dB
        for (double sr : {44100.0, 48000.0, 96000.0}) {
            const double a1 = aliasOf(sr, 112, 0, 1.f), a2 = aliasOf(sr, 112, 1, 1.f), a3 = aliasOf(sr, 112, 2, 0.1f);   // 5,27 kHz, drive maxi (douce, écrêtée) ou 0,1 (repliée : seuil -60 dB, ses rafales sont trop larges pour tout suréchantillonnage)
            double bass = -400; for (int sat = 0; sat < 3; ++sat) for (int note : {28, 36}) bass = std::max(bass, aliasOf(sr, note, sat, sat == 2 ? 0.9f : 1.f));
            std::printf("808 %.0f Hz : 5,27 kHz douce %.1f dB, ecretee %.1f dB, repliee(0,1) %.1f dB ; basses (41/65 Hz, drive maxi) pire %.1f dB\n", sr, a1, a2, a3, bass);
            CHECK(a1 < -70.0 && a2 < -70.0 && a3 < -60.0, "repliement 5 kHz a %.0f Hz : %.1f / %.1f / %.1f dB", sr, a1, a2, a3);
            CHECK(bass < -70.0, "repliement des basses a %.0f Hz : %.1f dB", sr, bass);
        }
        // latence : le premier echantillon non nul (clic) tombe exactement a la latence declaree
        for (double sr : {44100.0, 48000.0, 96000.0}) {
            o808::Engine e; e.prepare(sr); e.setParams(preset(1)); e.noteOn(33, 1.f);
            std::vector<float> y(4000); e.render(y.data(), 4000);
            int first = -1; for (int i = 0; i < 4000; ++i) if (y[(size_t)i] != 0.f) { first = i; break; }
            std::printf("808 %.0f Hz : latence declaree %d, premier echantillon non nul %d\n", sr, e.latency(), first);
            CHECK(first == e.latency() && e.latency() > 0, "latence 808 : declaree %d, mesuree %d", e.latency(), first);
        }
    }
    {   // vague 2B : saturation après l'enveloppe -> le taux d'harmoniques (H3..H20 / fondamental) baisse avec le niveau de la queue.
        // Preset « Trap saturée » sans clic ni triangle (tone 0 : son H2/H6.. linéaire fausserait la mesure), C2, 44,1 kHz.
        const double sr = 44100; o808::Engine e; e.prepare(sr);
        o808::Params p = preset(1); p.click = 0; p.tone = 0; e.setParams(p);
        e.noteOn(36, 1.f); std::vector<float> y((size_t)(sr * 2.0)); e.render(y.data(), (int)y.size());
        const double f0 = 440 * std::pow(2, (36 - 69) / 12.0);
        auto gz = [&](size_t s0, double f) { double re = 0, im = 0; const size_t n = 4096;
            for (size_t i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos(2 * 3.14159265358979 * (double)i / (double)(n - 1)), ph = 2 * 3.14159265358979 * f * (double)i / sr; re += w * y[s0 + i] * std::cos(ph); im += w * y[s0 + i] * std::sin(ph); }
            return re * re + im * im; };
        double hd[4]; const double ts[4] = {0.05, 0.3, 0.6, 1.2};
        for (int k = 0; k < 4; ++k) { const size_t s0 = (size_t)(ts[k] * sr); double H = 0; for (int h = 2; h <= 20; ++h) H += gz(s0, f0 * h);
            hd[k] = 10 * std::log10(H / gz(s0, f0)); std::printf("808 bloom : t=%.2f s, fond %.1f dB, harmoniques/fond %.1f dB\n", ts[k], 10 * std::log10(gz(s0, f0)) - 20 * std::log10(4096 / 4.0), hd[k]); }
        CHECK(hd[0] > -12.0, "attaque pas assez saturee (%.1f dB)", hd[0]);
        CHECK(hd[3] < hd[0] - 15.0, "pas de bloom : %.1f dB a 1,2 s contre %.1f dB a 50 ms", hd[3], hd[0]);
    }
    {   // lot E : relance immédiate. Une note qui interrompt la précédente part à l'échantillon près (fondu croisé de 2,5 ms) : plus de trou.
        const double sr = 48000;
        double gap[2];
        for (int k = 0; k < 2; ++k) {
            o808::Engine e; e.prepare(sr); o808::Params p; p.click = 0; p.punch = 0; p.drive = 0; p.len = 2; p.retrigMs = k ? 2.5f : 0.f; e.setParams(p);
            std::vector<float> y((size_t)sr); const int t2 = 14400;
            e.noteOn(69, 1.f); e.render(y.data(), t2); e.noteOff(69); e.noteOn(76, 1.f); e.render(y.data() + t2, (int)sr - t2);
            auto rms = [&](int i) { double s = 0; for (int j = 0; j < 48; ++j) s += (double)y[(size_t)(i + j)] * y[(size_t)(i + j)]; return std::sqrt(s / 48); };
            const double ref = [&] { double s = 0; for (int j = 0; j < 480; ++j) s += (double)y[(size_t)(t2 - 480 + j)] * y[(size_t)(t2 - 480 + j)]; return std::sqrt(s / 480); }();
            int under = 0; for (int i = t2 + e.latency(); i < t2 + e.latency() + 1440; i += 4) if (rms(i) < 0.3 * ref) under += 4;
            gap[k] = under / 48.0;
            CHECK(finite(y) && peakOf(y, 0, y.size()) <= 1.f, "relance : sortie non bornee");
        }
        std::printf("808 relance : duree sous 30 %% du niveau : ancien %.1f ms, fondu %.1f ms\n", gap[0], gap[1]);
        CHECK(gap[0] > 2.0 && gap[1] < 0.3, "relance : trou ancien %.1f ms, nouveau %.1f ms", gap[0], gap[1]);
    }
    {   // lot E : « Pliée » (foldScale 0,5) : erreur de repliement contre un rendu à 192 kHz décimé, alignement fractionnaire. Repliée d'origine : -25..-34 dB.
        auto render = [](double sr, int note, float drive, float fold, int n) { o808::Engine e; e.prepare(sr); o808::Params p; p.sat = 2; p.drive = drive; p.tone = 0.45f; p.cut = 1; p.click = 0; p.punch = 0; p.len = 8; p.foldScale = fold; e.setParams(p);
            std::vector<float> L((size_t)n); e.noteOn(note, 1.f); for (int i = 0; i < n; i += 256) e.render(&L[(size_t)i], std::min(256, n - i)); return L; };
        double worst[2] = {-400, -400};
        for (int k = 0; k < 2; ++k) for (int note : {33, 45}) {
            const float fold = k ? 0.5f : 1.f;
            auto a = render(48000, note, 1.f, fold, 48000), b = render(192000, note, 1.f, fold, 192000);
            const int T = 255; std::vector<double> h((size_t)T); double sum = 0;
            for (int i = 0; i < T; ++i) { const double m = i - (T - 1) / 2.0, x = 2 * 20000.0 / 192000.0 * m, sc = m == 0 ? 1 : std::sin(3.14159265358979 * x) / (3.14159265358979 * x), w = 0.42 - 0.5 * std::cos(2 * 3.14159265358979 * i / (T - 1)) + 0.08 * std::cos(4 * 3.14159265358979 * i / (T - 1)); h[(size_t)i] = sc * w; sum += h[(size_t)i]; }
            for (auto& x : h) x /= sum;
            std::vector<float> d(48000); for (int i = 0; i < 48000; ++i) { double acc = 0; for (int q = 0; q < T; ++q) { const int j = i * 4 + q - (T - 1) / 2; if (j >= 0 && j < 192000) acc += h[(size_t)q] * b[(size_t)j]; } d[(size_t)i] = (float)acc; }
            auto at = [&](double pos) { const int i = (int)std::floor(pos); const double f = pos - i, y0 = a[(size_t)i - 1], y1 = a[(size_t)i], y2 = a[(size_t)i + 1], y3 = a[(size_t)i + 2]; return y1 + 0.5 * f * (y2 - y0 + f * (2 * y0 - 5 * y1 + 4 * y2 - y3 + f * (3 * (y1 - y2) + y3 - y0))); };
            double best = 1e30;
            for (double lag = 30; lag <= 38; lag += 0.02) { double e2 = 0, r2 = 0; for (int i = 24000; i < 36000; ++i) { const double x = at(i + lag), y = d[(size_t)i]; e2 += (x - y) * (x - y); r2 += y * y; } best = std::min(best, e2 / r2); }
            worst[k] = std::max(worst[k], 10 * std::log10(best));
        }
        std::printf("808 repliement (erreur vs 192 kHz, drive 1, pire de A1/A2) : Repliee d'origine %.1f dB, Pliee %.1f dB\n", worst[0], worst[1]);
        CHECK(worst[1] < -40.0 && worst[1] < worst[0] - 15.0, "Pliee : erreur de repliement %.1f dB (origine %.1f)", worst[1], worst[0]);
    }
    std::printf(fails ? "orbe808_test : %d echec(s)\n" : "orbe808_test : OK\n", fails);
    return fails ? 1 : 0;
}
