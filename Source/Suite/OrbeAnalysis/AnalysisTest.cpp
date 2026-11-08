// Tests console de l'analyse Orbe (sans JUCE) : tempo, tonalité, LUFS, pic vrai, YIN, segmentation.
#include "OrbeAnalysis.h"
#include <cstdio>
#include <random>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC  " __VA_ARGS__); std::printf("\n"); } else { std::printf("ok     " __VA_ARGS__); std::printf("\n"); } } while (0)

static std::vector<float> clicks(double bpm, double sr, double sec) {
    std::vector<float> x((size_t)(sr * sec), 0.f); const double beat = 60.0 / bpm * sr; std::mt19937 rg(3); std::uniform_real_distribution<float> u(-1, 1);
    for (double p = 0; p < x.size() - 600; p += beat) for (int i = 0; i < 400; ++i) x[(size_t)p + (size_t)i] += (float)(0.8 * std::exp(-i / 40.0) * u(rg));
    return x;
}
static void addNote(std::vector<float>& x, double sr, double f, double t0, double dur, double amp = .3) {
    for (long i = 0; i < (long)(dur * sr); ++i) { const double t = i / sr, env = std::min(1.0, t / .01) * std::min(1.0, (dur - t) / .02); const long k = (long)(t0 * sr) + i; if (k < (long)x.size()) x[(size_t)k] += (float)(amp * env * std::sin(2 * oa::kPi * f * t)); }
}
int main() {
    const double sr = 44100;
    for (double bpm : {120.0, 140.0}) { auto x = clicks(bpm, sr, 30); auto t = oa::detectTempo(x.data(), (int)x.size(), sr); CHECK(std::abs(t.bpm - bpm) <= 1, "tempo clic %.0f BPM -> %.1f (confiance %.2f)", bpm, t.bpm, t.conf); }
    {   // accord de La mineur (A C E), enchaîné sur 12 s
        std::vector<float> x((size_t)(sr * 12), 0.f);
        for (double t0 = 0; t0 < 11; t0 += 2) for (double f : {220.0, 261.63, 329.63, 110.0}) { addNote(x, sr, f, t0, 1.9, .15); addNote(x, sr, f * 2, t0, 1.9, .05); }
        auto k = oa::detectKey(x.data(), (int)x.size(), sr);
        CHECK(k.pc == 9 && k.minor, "tonalite accord La mineur -> %s (%s, conf %.2f)", oa::keyLong(k.pc, k.minor).c_str(), oa::camelot(k.pc, k.minor).c_str(), k.conf);
        CHECK(oa::camelot(9, true) == "8A" && oa::camelot(0, false) == "8B" && oa::camelot(3, false) == "5B", "Camelot A min = 8A, C maj = 8B, Eb maj = 5B");
    }
    {   // LUFS : référence BS.1770 (0 dBFS 997 Hz sur un seul canal = −3,01 LUFS)
        oa::MixMeter m; m.prepare(48000); const int N = 48000 * 8;
        std::vector<float> a((size_t)N), z((size_t)N, 0.f); for (int i = 0; i < N; ++i) a[(size_t)i] = (float)(0.1 * std::sin(2 * oa::kPi * 997 * i / 48000.0));
        m.process(a.data(), z.data(), N); oa::MixSnap s; m.fill(s);
        CHECK(std::abs(s.lufsI - -23.0) <= .5, "LUFS sinus -20 dBFS 997 Hz (un canal) = %.2f (attendu -23,0)", s.lufsI);
        CHECK(std::abs(s.lufsM - -23.0) <= .5 && std::abs(s.lufsS - -23.0) <= .5, "momentane %.2f court terme %.2f", s.lufsM, s.lufsS);
        oa::MixMeter m2; m2.prepare(48000); std::vector<float> b((size_t)N); for (int i = 0; i < N; ++i) b[(size_t)i] = (float)(0.0707946 * std::sin(2 * oa::kPi * 997 * i / 48000.0));
        m2.process(b.data(), b.data(), N); m2.fill(s);
        CHECK(std::abs(s.lufsI - -23.0) <= .5, "LUFS sinus -23 dBFS stereo = %.2f (attendu -23,0)", s.lufsI);
        CHECK(s.corr > .99 && s.frames > 10, "correlation %.2f, %d trames spectrales", s.corr, s.frames);
    }
    {   // pic vrai : sinus à fs/4 déphasé de 45° -> échantillons à 0,707 mais crête réelle à 1,0
        oa::MixMeter m; m.prepare(48000); const int N = 48000; std::vector<float> a((size_t)N);
        for (int i = 0; i < N; ++i) a[(size_t)i] = (float)std::sin(oa::kPi / 2 * i + oa::kPi / 4);
        m.process(a.data(), a.data(), N); oa::MixSnap s; m.fill(s);
        CHECK(s.peak < -2.9 && s.tp > s.peak + 1.0 && s.tp < .3, "pic echantillon %.2f dBFS, pic vrai %.2f dBTP", s.peak, s.tp);
    }
    {   // YIN 220 Hz
        std::vector<float> x((size_t)(sr * 2), 0.f); addNote(x, sr, 220, 0, 2, .4);
        auto t = oa::analyzePitch(x.data(), (int)x.size(), sr); int ok = 0, tot = 0;
        for (auto& f : t.frames) if (f.t > .2 && f.t < 1.8) { ++tot; if (std::abs(f.f - 220) < 1.5) ++ok; }
        CHECK(tot > 0 && ok >= tot * .95, "YIN 220 Hz : %d/%d trames justes", ok, tot);
    }
    {   // segmentation : A3 C4 E4 A4 puis silence
        std::vector<float> x((size_t)(sr * 3), 0.f); const double fr[4] = {220, 261.63, 329.63, 440}; for (int i = 0; i < 4; ++i) addNote(x, sr, fr[i], .2 + i * .5, .4);
        auto t = oa::analyzePitch(x.data(), (int)x.size(), sr); oa::SegParams p; p.grid = 0; auto r = oa::segment(t, p);
        const int want[4] = {57, 60, 64, 69}; bool ok = r.notes.size() == 4; for (size_t i = 0; ok && i < 4; ++i) ok = r.notes[i].n == want[i];
        CHECK(ok, "segmentation melodie : %zu notes (attendu A3 C4 E4 A4), tonalite %s", r.notes.size(), oa::keyShort(r.key, r.mode == 0).c_str());
        for (auto& n : r.notes) std::printf("       n=%d  %.2f-%.2f s\n", n.n, n.s, n.e);
        p.oct = 1; p.grid = 1; r = oa::segment(t, p); CHECK(!r.notes.empty() && r.notes[0].n == 69, "octave +1 et grille 1/16 : premiere note %d", r.notes.empty() ? -1 : r.notes[0].n);
        std::vector<oa::MidiNote> mn; for (auto& n : r.notes) mn.push_back({n.s * 100 / 60, (n.e - n.s) * 100 / 60, n.n, .85});
        auto mid = oa::writeMidi(mn, 100); CHECK(mid.size() > 40 && std::memcmp(mid.data(), "MThd", 4) == 0 && mid[12] == 0x01 && mid[13] == 0xE0, "fichier MIDI %zu octets, PPQ 480", mid.size());
    }
    std::printf(fails ? "\n%d ECHEC(S)\n" : "\ntout est bon\n", fails);
    return fails ? 1 : 0;
}
