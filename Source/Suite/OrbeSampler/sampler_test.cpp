// Test console du moteur Orbe Sampler (sans JUCE) : cmake --build . --target orbe_sampler_test
#include <cstdio>
#include "PadEngine.h"
#include "TransformEngine.h"
using namespace osamp;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC  " __VA_ARGS__); std::printf("\n"); } else { std::printf("ok     " __VA_ARGS__); std::printf("\n"); } } while (0)
static bool finite(const std::vector<float>& v, float& pk) { pk = 0; for (float x : v) { if (!std::isfinite(x)) return false; pk = std::max(pk, std::fabs(x)); } return true; }

int main() {
    const double sr = 44100;
    // 1) clic à 100 BPM : le tempo détecté doit être ≈ 100
    for (double bpm : {100.0, 128.0, 80.0}) {
        Sample s; s.sr = sr; const int n = (int)(24 * sr);
        s.ch[0].assign((size_t)n, 0.f);
        const double per = 60.0 / bpm * sr;
        uint32_t rs = 1;
        for (double t = 0; t < n - 800; t += per) for (int i = 0; i < 600; ++i) { rs = rs * 1664525u + 1013904223u; s.ch[0][(size_t)(t + i)] += ((float)(rs >> 8) / 8388608.f - 1.f) * std::exp(-i / 90.f) * 0.8f; }
        s.ch[1] = s.ch[0];
        const auto t = tempo(s);
        CHECK(std::fabs(t.bpm - bpm) < 1.5, "tempo clic %.0f BPM -> %.1f (confiance %.2f)", bpm, t.bpm, t.conf);
    }
    // 2) accords générés
    auto chord = [&](std::initializer_list<int> notes) {
        Sample s; s.sr = sr; const int n = (int)(8 * sr); s.ch[0].assign((size_t)n, 0.f);
        for (int m : notes) { const double f = 440 * std::pow(2.0, (m - 69) / 12.0); for (int h = 1; h <= 5; ++h) for (int i = 0; i < n; ++i) s.ch[0][(size_t)i] += (float)(std::sin(2 * kPi * f * h * i / sr) / h * 0.2); }
        s.ch[1] = s.ch[0]; return s;
    };
    { auto k = detectKey(chord({48, 52, 55})); CHECK(k.pc == 0 && !k.minor, "accord Do majeur (C E G) -> %s%s %s", noteLetter(k.pc), k.minor ? "m" : "", camelot(k.pc, k.minor).c_str()); }
    { auto k = detectKey(chord({45, 48, 52})); CHECK(k.pc == 9 && k.minor, "accord La mineur (A C E) -> %s%s %s", noteLetter(k.pc), k.minor ? "m" : "", camelot(k.pc, k.minor).c_str()); }
    CHECK(camelot(0, false) == "8B" && camelot(9, true) == "8A" && camelot(7, false) == "9B", "camelot C=8B Am=8A G=9B");
    // 3) WSOLA conserve la durée demandée, pas de NaN
    {
        auto d = demoSample(sr);
        Chs c{d->ch[0], d->ch[1]};
        for (double f : {0.5, 0.8, 1.25, 2.0}) {
            auto o = stretch(c, f);
            float pk; bool ok = finite(o[0], pk);
            const double want = std::ceil(c[0].size() * f);
            CHECK(ok && std::fabs((double)o[0].size() - want) <= 1 && pk < 2.f, "WSOLA x%.2f : %zu -> %zu (attendu %.0f), pic %.2f", f, c[0].size(), o[0].size(), want, pk);
        }
        // hauteur conservée : le fondamental d'un sinus ne doit pas bouger
        Chs sine(2, std::vector<float>((size_t)(2 * sr)));
        for (size_t i = 0; i < sine[0].size(); ++i) sine[0][i] = sine[1][i] = (float)std::sin(2 * kPi * 440 * i / sr) * 0.5f;
        auto o = stretch(sine, 1.5);
        int zc = 0; for (size_t i = 10000; i + 1 < 60000; ++i) if (o[0][i] < 0 && o[0][i + 1] >= 0) ++zc;
        CHECK(std::fabs(zc / (50000.0 / sr) - 440.0) < 8, "WSOLA garde la hauteur : %.1f Hz", zc / (50000.0 / sr));
        // 4) démo : tempo / tonalité / transitoires
        Sample ds = *d; auto t = tempo(ds); auto k = detectKey(ds); auto on = onsets(mono(ds), sr, 0.55f, 16);
        CHECK(std::fabs(t.bpm - 92) < 2.5, "démo : tempo %.1f (92), tonalité %s%s %s, %zu repères", t.bpm, noteLetter(k.pc), k.minor ? "m" : "", camelot(k.pc, k.minor).c_str(), on.size());
        // 5) pads
        Bank bank; bank.pcm = d; bank.nsl = 16; for (int i = 0; i < 16; ++i) { bank.a[i] = d->size() * i / 16; bank.b[i] = d->size() * (i + 1) / 16; }
        PadEngine eng; eng.prepare(48000); eng.setTempo(92);
        std::vector<float> L(48000 * 3, 0.f), R = L;
        for (int i = 0; i < 16; ++i) { PadParams p; p.pitch = (float)(i - 8); p.volDb = 6; p.pan = i % 2 ? 80.f : -80.f; p.cut = i % 3 ? 0.5f : 1.f; p.crush = i % 4 ? 0.7f : 0.f; p.rv = 1; p.dl = 1; p.rev = i % 2; p.grp = i % 4; eng.trigger(&bank, i, 1.f, p, false); }
        for (int o2 = 0; o2 < (int)L.size(); o2 += 480) eng.render(L.data() + o2, R.data() + o2, 480);
        float pl, pr; bool ok = finite(L, pl) && finite(R, pr);
        float rms = 0; for (float v : L) rms += v * v;
        CHECK(ok && pl <= 1.f && pr <= 1.f && rms > 0.1f, "pads : sans NaN, pic %.3f / %.3f, énergie %.1f", pl, pr, rms);
        // 6) transformateur : chaque configuration reste finie et bornée
        Transformer tf;
        struct Cfg { const char* n; TParams p; };
        auto mk = [](auto f) { TParams p; f(p); return p; };
        std::vector<Cfg> cfgs = {
            {"brut", mk([](TParams&) {})},
            {"vinyle", mk([](TParams& p) { p.cut = .78f; p.crackle = .45f; p.wow = .18f; p.bits = 14; })},
            {"cassette", mk([](TParams& p) { p.cut = .6f; p.wow = .5f; p.bits = 12; p.rate = .6f; p.crackle = .08f; })},
            {"slowed", mk([](TParams& p) { p.semi = -3; p.verb = .5f; p.cut = .85f; })},
            {"indep+verb+tape", mk([](TParams& p) { p.mode = 1; p.semi = 4; p.speed = 80; p.verb = 1; p.tape = true; p.rev = true; p.loop = false; })},
            {"sync", mk([](TParams& p) { p.mode = 1; p.sync = true; p.hostBpm = 110; p.chop = -1; })},
        };
        for (auto& c : cfgs) {
            auto r = tf.render(d, c.p, 92, 9, true);
            float pk; bool fin = r && finite(r->pcm->ch[0], pk) && finite(r->pcm->ch[1], pk);
            float pk2 = 0; if (r) { finite(r->pcm->ch[0], pk); finite(r->pcm->ch[1], pk2); pk = std::max(pk, pk2); }
            CHECK(fin && pk <= 0.9701f && r->nsl > 0, "rendu %-16s : %.2f s, pic %.3f, %d tranches, %.1f BPM", c.n, r ? r->pcm->dur() : 0.0, pk, r ? r->nsl : 0, r ? r->bpm : 0.0);
        }
        { auto r = tf.render(d, cfgs[5].p, 92, 9, true); CHECK(std::fabs(r->pcm->dur() - d->dur() * 92 / 110) < 0.05, "étirement sur BPM hôte : %.2f s (attendu %.2f)", r->pcm->dur(), d->dur() * 92 / 110); }
    }
    std::printf(fails ? "\n%d ECHEC(S)\n" : "\nTOUT EST BON\n", fails);
    return fails ? 1 : 0;
}
