// Test console du moteur Orbe Sampler (sans JUCE) : cmake --build . --target orbe_sampler_test
#include <cstdio>
#include <chrono>
#include <thread>
#include "PadEngine.h"
#include "Swap.h"
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
    // 7) durée de vie des banques par générations (crash A10 / P1 / S1) : à lancer aussi sous ASan
    //    g++ -std=c++17 -g -fsanitize=address,undefined sampler_test.cpp -o t && ./t
    {
        auto mkBank = [&](int frames) {
            auto b = std::make_shared<Bank>();
            auto s = std::make_shared<Sample>(); s->sr = 48000.0;
            s->ch[0].assign((size_t)frames, 0.f);
            for (int i = 0; i < frames; ++i) s->ch[0][(size_t)i] = 0.3f * (float)std::sin(2 * kPi * 220.0 * i / 48000.0);
            s->ch[1] = s->ch[0];
            b->pcm = s; b->nsl = 1; b->a[0] = 0; b->b[0] = frames;
            return b;
        };
        // a) voix de 12 s (3 s lues à -24 dt) pendant que la banque est remplacée à chaque bloc : elle reste vivante tant qu'elle joue
        {
            Swap<Bank> sw; PadEngine eng; eng.prepare(48000);
            auto first = mkBank(3 * 48000);
            std::weak_ptr<const Bank> wf = first;
            sw.publish(std::move(first));
            std::vector<float> L(480), R(480);
            PadParams p; p.pitch = -24.f; p.relMs = 30.f;
            eng.trigger(sw.get(), 0, 1.f, p, false);
            sw.reportInUse(eng.oldestGen(sw.currentGen()));
            bool aliveWhilePlaying = true, readOk = true; int blocks = 0;
            float pk = 0;
            while (eng.activeVoices() > 0 && blocks < 4000) {
                sw.publish(mkBank(2000));                              // thread message : nouvelle banque
                if (eng.activeVoices() > 0 && wf.expired()) aliveWhilePlaying = false;
                std::fill(L.begin(), L.end(), 0.f); R = L;
                eng.render(L.data(), R.data(), 480);                    // thread audio : lit la banque d'origine
                for (float x : L) { if (!std::isfinite(x)) readOk = false; pk = std::max(pk, std::fabs(x)); }
                sw.reportInUse(eng.oldestGen(sw.currentGen()));
                ++blocks;
            }
            CHECK(aliveWhilePlaying && readOk && blocks > 1000 && pk > 0.05f, "banque : vivante pendant toute la voix (%d blocs = %.1f s, %d publications, pic %.2f)", blocks, blocks * 480 / 48000.0, blocks, pk);
            sw.publish(mkBank(2000));
            CHECK(wf.expired() && sw.liveCount() <= 2, "banque : libérée une fois la voix finie (%d objet(s) gardé(s))", sw.liveCount());
        }
        // b) prepare : les voix sont coupées, tout ce qui n'est pas courant part
        {
            Swap<Bank> sw; for (int i = 0; i < 5; ++i) sw.publish(mkBank(1000));
            CHECK(sw.liveCount() == 5, "tant que le thread audio n'a rien annoncé, rien n'est libéré (%d)", sw.liveCount());
            sw.quiesce();
            CHECK(sw.liveCount() == 1, "quiesce : seul l'objet courant reste (%d)", sw.liveCount());
        }
        // c) deux threads : l'audio lit en continu, le thread message publie sans arrêt (voix longues de 8 s, retriggers)
        {
            Swap<Bank> sw; sw.publish(mkBank(48000));
            std::atomic<bool> stop{false}; std::atomic<long> reads{0}; std::atomic<bool> bad{false};
            std::thread audio([&] {
                PadEngine eng; eng.prepare(48000);
                std::vector<float> L(128), R(128); long blk = 0;
                while (!stop.load()) {
                    const Bank* bk = sw.get();
                    PadParams p; p.pitch = -24.f;
                    if (blk % 37 == 0) eng.trigger(bk, 0, 1.f, p, false);
                    std::fill(L.begin(), L.end(), 0.f); R = L;
                    eng.render(L.data(), R.data(), 128);
                    for (float x : L) if (!std::isfinite(x)) bad = true;
                    sw.reportInUse(eng.oldestGen(sw.currentGen()));
                    ++blk; reads = blk;
                }
            });
            for (int i = 0; i < 3000; ++i) { sw.publish(mkBank(48000)); std::this_thread::sleep_for(std::chrono::microseconds(150)); }
            stop = true; audio.join();
            CHECK(!bad && reads > 100, "2 threads : %ld blocs lus pendant 3000 publications, historique %d objet(s)", reads.load(), sw.liveCount());
        }
    }
    {   // lot E : sorties séparées par pad. Un pad seul, sans envoi : sa sortie auxiliaire = le mix, au bit près ; deux pads : chacun sur sa sortie ; mix inchangé
        auto d = demoSample(44100.0);
        Bank bank; bank.pcm = d; bank.nsl = 16; for (int i = 0; i < 16; ++i) { bank.a[i] = d->size() * i / 16; bank.b[i] = d->size() * (i + 1) / 16; }
        const int N = 24000;
        auto run1 = [&](std::initializer_list<int> pads, bool stems, std::vector<float>& L, std::vector<std::vector<float>>& sl) {
            PadEngine e; e.prepare(48000); e.setTempo(92);
            for (int pd : pads) { PadParams p; p.pan = pd % 2 ? 40.f : -40.f; e.trigger(&bank, pd, .9f, p, false); }
            L.assign((size_t)N, 0.f); std::vector<float> R((size_t)N, 0.f);
            sl.assign(16, std::vector<float>((size_t)N, 0.f)); auto sr2 = sl;
            float* a[16]; float* b[16]; for (int k = 0; k < 16; ++k) { a[k] = sl[(size_t)k].data(); b[k] = sr2[(size_t)k].data(); }
            for (int o = 0; o < N; o += 480) {
                float* aa[16]; float* bb[16]; for (int k = 0; k < 16; ++k) { aa[k] = a[k] + o; bb[k] = b[k] + o; }
                e.render(L.data() + o, R.data() + o, 480, stems ? aa : nullptr, stems ? bb : nullptr);
            }
        };
        std::vector<float> L1, L2; std::vector<std::vector<float>> s1, s2;
        run1({3}, true, L1, s1); run1({3}, false, L2, s2);
        CHECK(L1 == L2, "mix inchange quand les sorties auxiliaires sont actives");
        run1({5, 9}, true, L1, s1);
        float pk5, pk9, pk0; finite(s1[5], pk5); finite(s1[9], pk9); finite(s1[0], pk0);
        CHECK(pk5 > .01f && pk9 > .01f && pk0 == 0.f, "deux pads : sorties 5 et 9 actives (%.3f, %.3f), sortie 0 muette (%.3f)", pk5, pk9, pk0);
        double d2 = 0; for (int i = 0; i < N; ++i) d2 = std::max(d2, (double)std::fabs(L1[(size_t)i] - (s1[5][(size_t)i] + s1[9][(size_t)i])));
        std::printf("       pads 5 + 9 : ecart max mix - (sortie 5 + sortie 9) = %.4f (reverb/delay/ecretage non lineaire)\n", d2);
    }
    {   // lot E : WSOLA à transitoires préservés. Burst de 2 ms toutes les 0,5 s : pic, pré-écho et doublon ; boucle de démo : durée, pic d'attaque, sauts
        const double sr2 = 44100; const int n = (int)(4 * sr2);
        for (double fac : {0.7, 0.9, 1.25, 1.6}) {
            Chs c(2, std::vector<float>((size_t)n, 0.f));
            for (int k = 0; k < 8; ++k) { const int st = (int)(k * sr2 / 2) + 1000; for (int j = 0; j < 88; ++j) c[0][(size_t)(st + j)] = c[1][(size_t)(st + j)] = (float)(0.8 * std::sin(2 * kPi * 3000 * j / sr2) * (1 - j / 88.0)); }
            double pkMin[2] = {9, 9}, preW[2] = {-200, -200}, postW[2] = {-200, -200};
            for (int tp = 0; tp < 2; ++tp) {
                auto o = tp ? stretchTP(c, fac, sr2) : stretch(c, fac);
                CHECK(std::fabs((double)o[0].size() - std::ceil(n * fac)) <= 1, "x%.2f %s : duree %zu", fac, tp ? "TP" : "WSOLA", o[0].size());
                for (int k = 1; k < 7; ++k) {
                    const int t = (int)((k * sr2 / 2 + 1000) * fac), a = std::max(0, t - (int)(0.08 * sr2)), b = std::min((int)o[0].size() - 1, t + (int)(0.08 * sr2));
                    int pi = a; for (int i = a; i < b; ++i) if (std::fabs(o[0][(size_t)i]) > std::fabs(o[0][(size_t)pi])) pi = i;
                    const double pk = std::fabs(o[0][(size_t)pi]); double pre = 0, post = 0;
                    for (int i = pi - (int)(0.06 * sr2); i < pi - (int)(0.004 * sr2); ++i) pre = std::max(pre, (double)std::fabs(o[0][(size_t)i]));
                    for (int i = pi + (int)(0.006 * sr2); i < pi + (int)(0.06 * sr2); ++i) post = std::max(post, (double)std::fabs(o[0][(size_t)i]));
                    pkMin[tp] = std::min(pkMin[tp], pk); preW[tp] = std::max(preW[tp], 20 * std::log10(pre / pk + 1e-9)); postW[tp] = std::max(postW[tp], 20 * std::log10(post / pk + 1e-9));
                }
            }
            std::printf("       x%.2f : pic des bursts WSOLA %.2f -> TP %.2f ; pre-echo %.1f -> %.1f dB ; doublon %.1f -> %.1f dB\n", fac, pkMin[0], pkMin[1], preW[0], preW[1], postW[0], postW[1]);
            CHECK(pkMin[1] > 0.7 && preW[1] < -40.0 && postW[1] < -40.0, "x%.2f TP : pic %.2f, pre-echo %.1f dB, doublon %.1f dB", fac, pkMin[1], preW[1], postW[1]);
        }
        auto d = demoSample(sr2); Chs c{d->ch[0], d->ch[1]};
        const auto att = attackPoints(mono(*d), sr2);
        double inJump = 0; for (size_t i = 1; i < c[0].size(); ++i) inJump = std::max(inJump, (double)std::fabs(c[0][i] - c[0][i - 1]));
        for (double fac : {0.5, 0.8, 1.25, 2.0}) {
            auto o = stretchTP(c, fac, sr2); float pk; const bool ok = finite(o[0], pk) && finite(o[1], pk);
            double jump = 0; for (size_t i = 1; i < o[0].size(); ++i) jump = std::max(jump, (double)std::fabs(o[0][i] - o[0][i - 1]));
            double a = 0, b = 0; for (int t : att) for (int j = 0; j < 220; ++j) { a = std::max(a, 0.0); }
            double sIn = 0, sOut = 0; for (int t : att) { double x = 0, y = 0; for (int j = 0; j < 220; ++j) { x = std::max(x, (double)std::fabs(c[0][(size_t)(t + j)])); const size_t q = (size_t)(t * fac) + (size_t)j; if (q < o[0].size()) y = std::max(y, (double)std::fabs(o[0][q])); } sIn += x; sOut += y; }
            (void)a; (void)b;
            CHECK(ok && std::fabs((double)o[0].size() - std::ceil(c[0].size() * fac)) <= 1 && jump < inJump * 1.2 && att.size() >= 8 && std::fabs(20 * std::log10(sOut / sIn)) < 1.0,
                  "demo x%.2f : %zu attaques, duree %zu, saut max %.3f (source %.3f), pic d'attaque %.2f dB", fac, att.size(), o[0].size(), jump, inJump, 20 * std::log10(sOut / sIn));
        }
    }
    std::printf(fails ? "\n%d ECHEC(S)\n" : "\nTOUT EST BON\n", fails);
    return fails ? 1 : 0;
}
