// Test console du moteur Orbe Chain (sans JUCE). Cible : orbechain_test (EXCLUDE_FROM_ALL). Usage : orbechain_test [prise.wav]
#include <complex>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <random>
#include "../../Kit/BypassFadeTest.h"
#include "Looks.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)

struct Wav { std::vector<float> l, r; double sr = 44100; };
static bool readWav(const char* path, Wav& w) {
    std::ifstream f(path, std::ios::binary); if (!f) return false;
    std::vector<char> b((std::istreambuf_iterator<char>(f)), {});
    int ch = 2, bits = 16; size_t pos = 12;
    while (pos + 8 <= b.size()) {
        uint32_t sz; std::memcpy(&sz, &b[pos + 4], 4);
        if (!std::memcmp(&b[pos], "fmt ", 4)) { uint16_t c, bt; uint32_t sr; std::memcpy(&c, &b[pos + 10], 2); std::memcpy(&sr, &b[pos + 12], 4); std::memcpy(&bt, &b[pos + 22], 2); ch = c; bits = bt; w.sr = sr; }
        if (!std::memcmp(&b[pos], "data", 4) && bits == 16) {
            const size_t n = std::min<size_t>(sz, b.size() - pos - 8) / 2 / (size_t)ch;
            for (size_t i = 0; i < n; ++i) { int16_t a, c2; std::memcpy(&a, &b[pos + 8 + i * 2 * (size_t)ch], 2); std::memcpy(&c2, &b[pos + 8 + i * 2 * (size_t)ch + (ch > 1 ? 2 : 0)], 2); w.l.push_back(a / 32768.f); w.r.push_back(c2 / 32768.f); }
            return true;
        }
        pos += 8 + sz + (sz & 1);
    }
    return false;
}

static void run(oc::Chain& c, const std::vector<float>& l, const std::vector<float>& r, std::vector<float>& oL, std::vector<float>& oR) {
    oL.assign(l.size(), 0.f); oR = oL;
    for (size_t p = 0; p < l.size(); p += 480) { const int n = (int)std::min<size_t>(480, l.size() - p); c.process(l.data() + p, r.data() + p, oL.data() + p, oR.data() + p, n); }
}
static float rmsDb(const std::vector<float>& l, const std::vector<float>& r, size_t a = 0) {
    double e = 0; for (size_t i = a; i < l.size(); ++i) e += 0.5 * ((double)l[i] * l[i] + (double)r[i] * r[i]);
    return (float)(10 * std::log10(e / (double)(l.size() - a) + 1e-12));
}
// Piste de hauteur (midi, 0 = non voisé) au pas du moteur, avec le détecteur YIN d'origine.
static std::vector<float> pitchTrack(const float* x, size_t n, float sr) {
    voile::Yin y; y.init(sr); std::vector<float> o; const int hop = std::max(64, (int)(0.0053 * sr)); float acc = 0; int dc = 0, hc = 0;
    for (size_t i = 0; i < n; ++i) {
        acc += x[i]; if (++dc >= y.dec) { y.push(acc / (float)y.dec); acc = 0; dc = 0; }
        if (++hc >= hop) { hc = 0; const float f = y.analyze(); o.push_back(f > 0 ? 69.f + 12.f * std::log2(f / 440.f) : 0.f); }
    }
    return o;
}
// Écart de hauteur entre entrée et sortie (sortie alignée par la latence) : médiane en cents et part de trames déplacées de > 30 cents.
static void pitchDiff(const std::vector<float>& in, const std::vector<float>& out, int lat, float sr, float& medCents, float& frac, int* cnt = nullptr) {
    const size_t n = std::min(in.size(), out.size() - (size_t)lat);
    const auto a = pitchTrack(in.data(), n, sr), b = pitchTrack(out.data() + lat, n, sr);
    std::vector<float> d; int moved = 0;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) if (a[i] > 0 && b[i] > 0) { const float c = std::fabs(a[i] - b[i]) * 100.f; d.push_back(c); moved += c > 30.f; }
    std::sort(d.begin(), d.end()); if (cnt) *cnt = (int)d.size(); medCents = d.empty() ? -1.f : d[d.size() / 2]; frac = d.empty() ? 0.f : (float)moved / (float)d.size();
}
static std::vector<float> voice(const std::vector<float>& midi, float noteSec, float sr, float cents) {   // timbre harmonique avec vibrato léger
    std::vector<float> x; double ph = 0;
    for (float m : midi) for (int i = 0; i < (int)(noteSec * sr); ++i) {
        const float t = (float)x.size() / sr, f = 440.f * std::pow(2.f, (m + cents / 100.f + 0.12f * std::sin(2 * oc::kPi * 5.f * t) - 69.f) / 12.f);
        ph += 2 * M_PI * f / sr; float v = 0; for (int h = 1; h <= 6; ++h) v += std::sin(h * ph) / (float)h;
        x.push_back(0.25f * v * (i < 400 ? (float)i / 400.f : 1.f));
    }
    return x;
}

int main(int argc, char** argv) {
    const char* wavPath = argc > 1 ? argv[1] : "/tmp/claude-0/-home-claude/12ae9892-af71-5ca8-a97b-93f9fd958b0b/scratchpad/prise.wav";
    Wav take; const bool haveTake = readWav(wavPath, take);
    CHECK(haveTake, "prise.wav illisible : %s", wavPath);
    const float ceilLin = std::pow(10.f, -1.f / 20.f) + 1e-4f;
    const int NL = oc::numLooks();

    { // catalogue : les 10 looks d'origine gardent leur index (état sauvegardé), les nouveaux suivent ; textes tenant dans l'afficheur
        static const char* const orig[10] = {"Turbine", "Béton", "Nacre", "Verre Pilé", "Cave", "Combiné", "Alcôve", "Braise", "Sépia", "Satin"};
        CHECK(NL >= 24 && NL <= 30, "nombre de looks inattendu : %d", NL);
        for (int k = 0; k < 10; ++k) CHECK(!std::strcmp(oc::looks()[(size_t)k].name, orig[k]), "look d'origine déplacé ou renommé à l'index %d", k);
        auto nChars = [](const char* s) { int n = 0; for (; *s; ++s) n += (*s & 0xC0) != 0x80; return n; };
        for (int k = 0; k < NL; ++k) {
            const auto& L = oc::looks()[(size_t)k];
            CHECK(L.name && L.tag && L.cat && *L.name && *L.tag, "look %d : nom, description ou catégorie vide", k);
            CHECK(nChars(L.name) <= 14, "nom trop long (afficheur) : %s", L.name); CHECK(nChars(L.tag) <= 46, "description trop longue (afficheur) : %s", L.tag);
            for (int j = 0; j < k; ++j) CHECK(std::strcmp(L.name, oc::looks()[(size_t)j].name), "nom en double : %s", L.name);
            for (float d : L.def) CHECK(d >= 0.f && d <= 1.f, "macro par défaut hors 0..1 : %s", L.name);
            CHECK(L.trimDb <= 0.f && L.trimDb >= -16.f && L.bias >= 0.f && L.bias <= 4.f, "trim/biais hors bornes : %s", L.name);
            if (k >= 10) CHECK(std::strcmp(L.cat, "Classiques"), "nouveau look sans catégorie : %s", L.name);
            for (const char* q = L.name; *q; ++q) CHECK((unsigned char)*q >= 32, "caractère de contrôle dans %s", L.name);
        }
        // latence déclarée : identique pour tous les looks et à toutes les fréquences ; mix 0 = entrée exactement retardée de cette latence
        for (double sr : {44100.0, 48000.0, 96000.0}) {
            oc::Chain ref; ref.prepare(sr); const int lat = ref.latency();
            const size_t N = (size_t)(sr * 0.9); std::vector<float> s(N); for (size_t i = 0; i < N; ++i) s[i] = 0.3f * std::sin(2 * oc::kPi * 440.f * (float)i / (float)sr) + 0.1f * std::sin(2 * oc::kPi * 2713.f * (float)i / (float)sr);
            for (int k = 0; k < NL; ++k) {
                oc::Chain c; c.prepare(sr); oc::S m0 = oc::lookDefaults(k); m0.mix = 0; m0.out = 0; c.set(m0);
                std::vector<float> oL, oR; run(c, s, s, oL, oR);
                double e = 0; int n = 0; for (size_t i = (size_t)(sr * 0.5); i < N; ++i) { const double v = oL[i] - s[i - (size_t)lat]; e += v * v; ++n; }
                CHECK(c.latency() == lat, "latence variable selon le look %s (%d != %d)", oc::looks()[(size_t)k].name, c.latency(), lat);
                CHECK(e / n < 1e-8, "mix 0 != entrée retardée de la latence déclarée : %s à %g Hz (%.2e)", oc::looks()[(size_t)k].name, sr, e / n);
            }
        }
    }
    // 1. NaN / pic borné : chaque look, macros par défaut et extrêmes, 3 fréquences d'échantillonnage
    for (double sr : {44100.0, 48000.0, 96000.0}) {
        std::mt19937 rng(5); std::uniform_real_distribution<float> u(-1.f, 1.f);
        std::vector<float> x((size_t)(sr * 3)); for (size_t i = 0; i < x.size(); ++i) x[i] = 0.7f * std::sin(2 * oc::kPi * 180.f * (float)i / (float)sr) * (0.5f + 0.5f * std::sin(2 * oc::kPi * 2.f * (float)i / (float)sr)) + 0.3f * u(rng);
        for (int k = 0; k < NL; ++k) for (int corner = 0; corner < 3; ++corner) {
            oc::Chain c; c.prepare(sr); c.setAutoLevel(true);
            oc::S s = corner == 0 ? oc::lookDefaults(k) : oc::lookSettings(k, corner == 1 ? 1.f : 0.f, corner == 1 ? 1.f : 0.f, corner == 1 ? 1.f : 0.f, corner == 1 ? 1.f : 0.f);
            std::vector<float> oL, oR; c.set(s); run(c, x, x, oL, oR);
            float pk = 0; bool ok = true; for (size_t i = 0; i < oL.size(); ++i) { ok &= std::isfinite(oL[i]) && std::isfinite(oR[i]); pk = std::max(pk, std::max(std::fabs(oL[i]), std::fabs(oR[i]))); }
            CHECK(ok, "NaN look %d corner %d sr %g", k, corner, sr); CHECK(pk <= ceilLin, "pic %f > plafond look %d corner %d sr %g", pk, k, corner, sr);
        }
    }
    { // réglages extrêmes de l'Atelier : tout actif, bornes max
        oc::S s; for (float* f : {&s.eqOn, &s.cpOn, &s.saOn, &s.tnOn, &s.dsOn, &s.dbOn, &s.dyOn, &s.rvOn}) *f = 1;
        s.hp = 400; s.lsG = 15; s.b1G = 15; s.b1Q = 8; s.b2G = -15; s.b2Q = 8; s.hsG = 15; s.hsF = 16000; s.thr = -60; s.ratio = 20; s.atk = 0.1f; s.rel = 5; s.mk = 24; s.drive = 1; s.crush = 1; s.bias = 1;
        s.tnSpeed = 1; s.shift = 12; s.dsAmt = 1; s.dbA = 12; s.dbB = -12; s.dyFb = 0.9f; s.dyMix = 1; s.rvSize = 8; s.rvMix = 1; s.rvDamp = 0.95f; s.out = 12; s.key = 3;
        std::vector<float> x((size_t)(48000 * 3)); std::mt19937 rng(9); std::uniform_real_distribution<float> u(-1.f, 1.f); for (auto& v : x) v = 0.9f * u(rng);
        oc::Chain c; c.prepare(48000); c.set(s); std::vector<float> oL, oR; run(c, x, x, oL, oR);
        bool ok = true; float pk = 0; for (size_t i = 0; i < oL.size(); ++i) { ok &= std::isfinite(oL[i]); pk = std::max(pk, std::fabs(oL[i])); }
        CHECK(ok, "NaN en réglages extrêmes"); CHECK(pk <= ceilLin, "pic extrême %f", pk);
    }

    // 2. latence = latence déclarée (chaîne neutre), 3. mix 0 = sec retardé, 4. bypass transparent
    for (double sr : {44100.0, 48000.0}) {
        const int N = (int)(sr * 2); std::vector<float> s((size_t)N); for (int i = 0; i < N; ++i) s[(size_t)i] = 0.3f * std::sin(2 * oc::kPi * 1000.f * (float)i / (float)sr) + 0.2f * std::sin(2 * oc::kPi * 3777.f * (float)i / (float)sr);
        oc::Chain c; c.prepare(sr); std::vector<float> oL, oR; c.set(oc::S{}); run(c, s, s, oL, oR);
        int best = -1; double be = 1e9; for (int d = 0; d < 4000; ++d) { double err = 0; for (int i = 20000; i < 30000; ++i) { const double v = oL[(size_t)i] - s[(size_t)(i - d)]; err += v * v; } if (err < be) { be = err; best = d; } }
        std::printf("sr %g : latence mesurée %d, déclarée %d, résidu %.2e\n", sr, best, c.latency(), be / 10000);
        CHECK(best == c.latency(), "latence %d != %d", best, c.latency()); CHECK(be / 10000 < 1e-6, "chaîne neutre non transparente");
        oc::Chain c2; c2.prepare(sr); oc::S m0 = oc::lookDefaults(0); m0.mix = 0; m0.out = 0; c2.set(m0); run(c2, s, s, oL, oR);
        double e = 0; for (int i = 30000; i < 60000; ++i) { const double v = oL[(size_t)i] - s[(size_t)(i - c2.latency())]; e += v * v; }
        CHECK(e / 30000 < 1e-8, "mix 0 != sec retardé (%.2e)", e / 30000);
        oc::BypassDelay b; b.init(c.latency()); std::vector<float> a(s.begin(), s.begin() + 6000), cc = a; b.process(a.data(), cc.data(), 6000, false);
        bool ok = true; for (int i = 0; i < 6000; ++i) ok &= a[(size_t)i] == (i >= c.latency() ? s[(size_t)(i - c.latency())] : 0.f) && cc[(size_t)i] == a[(size_t)i];
        CHECK(ok, "bypass non transparent");
    }

    if (haveTake) {
        const float sr = (float)take.sr; std::vector<float> mono(take.l.size()); for (size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * (take.l[i] + take.r[i]);
        std::printf("prise : %.1f s à %g Hz, %.1f dB RMS\n", mono.size() / sr, take.sr, rmsDb(take.l, take.r));
        // 5. auto-key sans fiabilité -> hauteur inchangée (YIN)
        oc::S hard; hard.tnOn = 1; hard.tnAmt = 1; hard.tnSpeed = 1; hard.key = 0;
        { // (a) 0,45 s de prise : trop court pour qu'une tonalité soit fiable
            const size_t n = (size_t)(1.0f * sr), o0 = (size_t)(3.0f * sr); std::vector<float> a(mono.begin() + (long)o0, mono.begin() + (long)(o0 + n)), oL, oR;   // début de la voix, trop court pour une tonalité fiable
            oc::Chain c; c.prepare(sr); c.set(hard); run(c, a, a, oL, oR);
            float med, fr; int cnt; pitchDiff(a, oL, c.latency(), sr, med, fr, &cnt);
            std::printf("auto-key (a) début de prise (1 s) : %d trames voisées, tonalité connue=%d, écart médian %.1f cents, déplacées %.1f %%\n", cnt, c.keyKnown(), med, fr * 100);
            CHECK(cnt >= 10, "trop peu de trames voisées pour conclure");
            CHECK(!c.keyKnown(), "tonalité jugée fiable sur 1 s"); CHECK(med >= 0 && med < 15.f && fr < 0.02f, "hauteur modifiée sans tonalité fiable");
        }
        { // (b) gamme chromatique en boucle, désaccordée : histogramme plat -> aucune tonalité fiable
            std::vector<float> notes; for (int rep = 0; rep < 4; ++rep) for (int k = 0; k < 12; ++k) notes.push_back(55.f + (float)k);
            const auto a = voice(notes, 0.25f, 48000.f, 38.f); std::vector<float> oL, oR;
            oc::Chain c; c.prepare(48000); c.set(hard); run(c, a, a, oL, oR);
            float med, fr; pitchDiff(a, oL, c.latency(), 48000.f, med, fr);
            std::printf("auto-key (b) chromatique : tonalité connue=%d, écart médian %.1f cents, trames déplacées %.1f %%\n", c.keyKnown(), med, fr * 100);
            CHECK(!c.keyKnown(), "tonalité fiable sur une gamme chromatique"); CHECK(med >= 0 && med < 15.f && fr < 0.02f, "hauteur modifiée (chromatique)");
        }
        { // (c) prise complète : si la tonalité devient fiable, la correction n'est autorisée qu'ensuite (information)
            oc::Chain c; c.prepare(sr); c.set(hard); std::vector<float> oL, oR; run(c, mono, mono, oL, oR);
            float med, fr; pitchDiff(mono, oL, c.latency(), sr, med, fr);
            std::printf("auto-key (c) prise entière : tonalité connue=%d (k=%d sc=%d), écart médian %.1f cents, déplacées %.1f %%\n", c.keyKnown(), c.detectedKey(), c.detectedScale(), med, fr * 100);
        }
        { // (d) contrôle positif : mélodie en Do majeur désaccordée de +38 cents, tonalité fiable -> correction vers la grille
            std::vector<float> notes; const int mel[] = {60, 64, 67, 64, 69, 67, 62, 65, 67, 64, 60, 62, 64, 67, 72, 69, 67, 64, 62, 60};
            for (int rep = 0; rep < 2; ++rep) for (int m : mel) notes.push_back((float)m);
            const auto a = voice(notes, 0.3f, 48000.f, 38.f); std::vector<float> oL, oR;
            oc::Chain c; c.prepare(48000); c.set(hard); run(c, a, a, oL, oR);
            const auto po = pitchTrack(oL.data() + c.latency() + 120000, oL.size() - (size_t)c.latency() - 120000, 48000.f);
            double err = 0; int nn = 0; for (float m : po) if (m > 0) { err += std::fabs(m - std::round(m)); ++nn; }
            std::printf("auto-key (d) positif : tonalité connue=%d, erreur résiduelle à la grille %.0f cents (entrée : 38)\n", c.keyKnown(), nn ? err / nn * 100 : -1);
            CHECK(c.keyKnown() && nn > 50 && err / nn < 0.20, "correction attendue absente quand la tonalité est fiable");
        }
        // 6. niveaux : chaque look reste à ±2 dB (looks d'origine) ou ±3 dB (nouveaux) du niveau d'entrée, sans NaN ni crête au-dessus du plafond (Looks : niveau auto ; Atelier : correction statique)
        const float inDb = rmsDb(take.l, take.r);
        std::vector<float> sn((size_t)(sr * 3)); for (size_t i = 0; i < sn.size(); ++i) sn[i] = 0.5012f * std::sin(2 * oc::kPi * 220.f * (float)i / sr);   // sinus à -6 dBFS
        float worstV = 0.f, worstS = 0.f;
        for (int mode = 0; mode < 3; ++mode) {   // 0 Looks auto v1, 1 Atelier (trim), 2 Looks auto v2 (lent, borné ±3 dB)
            std::printf("niveau %s (entrée %.1f dB) :", mode == 1 ? "Atelier (trim)" : mode == 2 ? "Looks (auto v2)" : "Looks (auto)", inDb);
            for (int k = 0; k < NL; ++k) {
                const char* nm = oc::looks()[(size_t)k].name; const float tol = k < 10 ? 2.f : 3.f;
                oc::Chain c; c.prepare(sr); c.setAutoLevel(mode != 1); oc::S s = oc::lookDefaults(k); if (mode != 1) s.out = 0; if (mode == 2) { s.agRise = 2.f; s.agFall = 0.8f; s.agRange = 3.f; } c.set(s);
                std::vector<float> oL, oR; run(c, take.l, take.r, oL, oR);
                const float d = rmsDb(oL, oR, (size_t)(sr * 1.5f)) - rmsDb(take.l, take.r, (size_t)(sr * 1.5f));
                float pk = 0; bool ok = true; for (size_t i = 0; i < oL.size(); ++i) { ok &= std::isfinite(oL[i]) && std::isfinite(oR[i]); pk = std::max(pk, std::max(std::fabs(oL[i]), std::fabs(oR[i]))); }
                std::printf(" %s %+.1f", nm, d); worstV = std::max(worstV, std::fabs(d));
                CHECK(std::fabs(d) <= tol, "niveau hors ±%.0f dB : %s %s %+.1f", tol, mode == 1 ? "Atelier" : "Looks", nm, d);
                CHECK(ok, "NaN sur la prise : %s %s", mode == 1 ? "Atelier" : "Looks", nm); CHECK(pk <= ceilLin, "crête %.2f dBFS > -1 dBFS sur la prise : %s", 20 * std::log10(pk), nm);
                if (mode == 2) { const float gdb = c.autoGainDb(); const float st = s.agInit; CHECK(gdb >= st - 3.05f && gdb <= st + 3.05f, "gain auto v2 hors ±3 dB du statique : %s %.1f / %.1f", nm, gdb, st); }
                // sinus à -6 dBFS : ni NaN ni crête > -1 dBFS (les deux modes) ; niveau auto à ±3 dB (le trim statique de l'Atelier est réglé pour une voix, pas pour un sinus)
                oc::Chain c2; c2.prepare(sr); c2.setAutoLevel(mode != 1); c2.set(s); std::vector<float> pL, pR; run(c2, sn, sn, pL, pR);
                float ps = 0; bool oks = true; for (size_t i = 0; i < pL.size(); ++i) { oks &= std::isfinite(pL[i]) && std::isfinite(pR[i]); ps = std::max(ps, std::max(std::fabs(pL[i]), std::fabs(pR[i]))); }
                const float ds = rmsDb(pL, pR, (size_t)(sr * 1.5f)) - rmsDb(sn, sn, (size_t)(sr * 1.5f));
                CHECK(oks, "NaN sur le sinus : %s", nm); CHECK(ps <= ceilLin, "crête %.2f dBFS > -1 dBFS sur le sinus : %s %s", 20 * std::log10(ps), mode == 1 ? "Atelier" : "Looks", nm);
                if (mode == 0) { worstS = std::max(worstS, std::fabs(ds)); CHECK(std::fabs(ds) <= 3.f, "niveau du sinus hors ±3 dB : %s %+.1f", nm, ds); }
            }
            std::printf("\n");
        }
        std::printf("écart de niveau maximal sur la prise %.1f dB ; sinus -6 dBFS (Looks auto) %.1f dB\n", worstV, worstS);
    }
    { // chantier 12 : saturation suréchantillonnée. Sinus 5 kHz poussé à fond (drive 1, biais 0,5) : repliement < -70 dB ; latence déclarée tenue
        for (double sr : {44100.0, 48000.0, 96000.0}) {
            const double f0 = 5000.0; const size_t N = 65536;
            oc::S s; s.saOn = 1; s.drive = 1.f; s.bias = 0.5f; s.saMix = 1.f; s.lmOn = 0; s.mix = 1.f;
            oc::Chain c; c.prepare(sr); c.set(s);
            std::vector<float> x((size_t)(2 * N + 40000)); for (size_t i = 0; i < x.size(); ++i) x[i] = (float)(0.9 * std::sin(2.0 * 3.14159265358979 * f0 * (double)i / sr));
            std::vector<float> oL, oR; run(c, x, x, oL, oR);
            std::vector<std::complex<double>> z(N);
            for (size_t i = 0; i < N; ++i) { const double w = 0.35875 - 0.48829 * std::cos(2 * 3.14159265358979 * (double)i / (double)(N - 1)) + 0.14128 * std::cos(4 * 3.14159265358979 * (double)i / (double)(N - 1)) - 0.01168 * std::cos(6 * 3.14159265358979 * (double)i / (double)(N - 1)); z[i] = w * oL[oL.size() - N + i]; }
            for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(z[i], z[j]); }
            for (size_t len = 2; len <= N; len <<= 1) { const std::complex<double> wl = std::polar(1.0, -2 * 3.14159265358979 / (double)len); for (size_t i = 0; i < N; i += len) { std::complex<double> w(1); for (size_t k = 0; k < len / 2; ++k) { const auto u = z[i + k], t = z[i + k + len / 2] * w; z[i + k] = u + t; z[i + k + len / 2] = u - t; w *= wl; } } }
            double pf = 0, worst = 0;
            for (size_t k = 1; k < N / 2; ++k) { const double f = (double)k * sr / (double)N, p2 = std::norm(z[k]); const int h = (int)std::lround(f / f0); if (std::fabs(f - h * f0) < 60.0) { if (h == 1) pf += p2; continue; } if (f < 30.0 || f > 20000.0) continue; worst = std::max(worst, p2); }
            const double db = 10 * std::log10(worst / pf + 1e-30);
            // latence : impulsion faible (saturation quasi linéaire) -> pic à la latence déclarée
            oc::Chain c3; c3.prepare(sr); c3.set(s); std::vector<float> imp(20000, 0.f); imp[5000] = 0.01f; std::vector<float> iL, iR; run(c3, imp, imp, iL, iR);
            size_t pk = 0; for (size_t i = 0; i < iL.size(); ++i) if (std::fabs(iL[i]) > std::fabs(iL[pk])) pk = i;
            std::printf("saturation x10 + biais, sinus 5 kHz à %g Hz : repliement %.1f dB ; latence déclarée %d, impulsion mesurée %d\n", sr, db, c.latency(), (int)pk - 5000);
            CHECK(db < -70.0, "repliement de la saturation de la chaîne : %.1f dB à %g Hz", db, sr);
            CHECK(std::abs((int)pk - 5000 - c.latency()) <= 1, "latence avec saturation : %d != %d", (int)pk - 5000, c.latency());
        }
    }
    { // bypass honoré (même Fader que processBlock) : sortie = entrée retardée de la latence (< -120 dB), saut au basculement < -60 dBFS
        oc::Chain c; oc::S s = oc::lookDefaults(2); std::vector<float> sc(4096);
        auto m = bypassfx::measure(48000.0, c.latency(), 4096, [&] { c.prepare(48000.0); c.setAutoLevel(true); c.set(s); }, [&](float* l, float* r, int n) { c.process(l, r, l, r ? r : sc.data(), n); });
        std::printf("bypass %s : latence %d, erreur %.1f dB, saut au basculement %.1f dBFS (signal seul : %.1f)\n", "OrbeChain (Looks, Atelier)", c.latency(), m.exactDb, m.jumpDb, m.plainDb);
        CHECK(m.exactDb < -120.0, "bypass non exact : %.1f dB", m.exactDb); CHECK(m.jumpDb < -60.0, "clic au basculement : %.1f dBFS", m.jumpDb);
    }
    { // Delay::read : retard fractionnaire juste au-dessus d'un entier (pré-délai de reverb 20 ms = 960,00004 éch.) -> jamais de pic.
        // Avant : p arrondi à n au repli donnait f = n, soit un pic ×7 700 une fois par tour de tampon (« claques » dans la reverb).
        voile::Delay d; d.init((int)(0.16 * 48000) + 4); float mx = 0.f;
        const float ds = std::clamp(20.f, 0.f, 150.f) * 1e-3f * 48000.f;
        for (int i = 0; i < 40000; ++i) { d.push(0.5f * std::sin(0.01f * (float)i)); mx = std::max(mx, std::fabs(d.read(ds))); }
        std::printf("Delay::read au repli : crête %.3f (entrée 0,5)\n", mx);
        CHECK(mx <= 0.501f, "pic de lecture au repli du tampon : %.2f", mx);
    }
    { // « claques » : voix rap (plosives après silence, sibilantes, voyelles) à -6 dBFS crête, limiteur coupé.
      // Aucun look ne doit dépasser le plafond de plus de 3 dB (avant correctifs : +13 dB, écrêté) ni rejouer d'attaque en octave.
        const float sr = 48000.f; std::mt19937 rng(3); std::normal_distribution<float> N(0.f, 1.f);
        std::vector<float> x;
        for (int ph = 0; ph < 4; ++ph) {
            x.insert(x.end(), (size_t)(0.5f * sr), 0.f);
            for (int k = 0; k < 6; ++k) {
                onek::Biquad h; h.highpass(sr, 1500.f); h.reset();
                for (int i = 0; i < (int)(0.012f * sr); ++i) x.push_back(0.5f * h.process((i < 150 ? 1.f : 0.3f) * N(rng)));
                double p = 0; const float f0 = 140.f + 10.f * (float)k;
                for (int i = 0; i < (int)(0.14f * sr); ++i) { p += f0 / sr; const float e = std::min(1.f, std::min((float)i / (0.004f * sr), (0.14f * sr - (float)i) / (0.02f * sr)));
                    x.push_back(0.4f * e * (float)(2.0 * (p - std::floor(p)) - 1.0)); }
            }
        }
        x.insert(x.end(), (size_t)(0.5f * sr), 0.f);
        float pk = 0.f; for (float v : x) pk = std::max(pk, std::fabs(v)); for (auto& v : x) v *= 0.5f / pk;
        float worst = -99.f; const char* wn = "";
        for (int k = 0; k < oc::numLooks(); ++k) {
            oc::S s = oc::lookDefaults(k); s.out = 0; s.lmOn = 0; const float ceilLin = std::pow(10.f, -1.f / 20.f);
            oc::Chain c; c.prepare(sr); c.setAutoLevel(true); c.set(s); std::vector<float> oL, oR; run(c, x, x, oL, oR);
            float m = 0.f; for (size_t i = 0; i < oL.size(); ++i) m = std::max(m, std::max(std::fabs(oL[i]), std::fabs(oR[i])));
            const float over = 20.f * std::log10(m / ceilLin + 1e-9f);
            if (over > worst) { worst = over; wn = oc::looks()[(size_t)k].name; }
            CHECK(over <= 3.f, "claque : %s dépasse le plafond de %.1f dB sans limiteur", oc::looks()[(size_t)k].name, over);
        }
        std::printf("claques (voix rap -6 dBFS, limiteur coupé) : pire dépassement du plafond %+.1f dB (%s)\n", worst, wn);
    }
    { // « De-ess avant comp » (Atelier) : l'interrupteur change bien l'ordre (rendu différent), le de-esser agit toujours dans les deux ordres et la voyelle ne bouge pas
        const float sr = 48000.f; std::mt19937 rng(5); std::normal_distribution<float> N(0.f, 1.f); std::vector<float> x;
        for (int ph = 0; ph < 6; ++ph) {
            onek::Biquad h; h.highpass(sr, 5500.f); h.reset();
            for (int i = 0; i < (int)(0.09f * sr); ++i) x.push_back(0.35f * h.process(N(rng)));
            double p = 0; for (int i = 0; i < (int)(0.25f * sr); ++i) { p += 150.0 / sr; x.push_back(0.3f * (float)std::sin(2 * 3.14159265 * p) * std::min(1.f, (float)i / 200.f)); }
        }
        double sib[3], vow[3];   // 0 sans de-ess, 1 ordre d'origine, 2 de-ess avant comp
        for (int f = 0; f < 3; ++f) {
            oc::S s; s.eqOn = 0; s.cpOn = 1; s.thr = -24; s.ratio = 4; s.atk = 5; s.rel = 80; s.mk = 6; s.cpMix = 1; s.saOn = 1; s.drive = 0.5f; s.saMix = 1; s.dsOn = f > 0; s.dsF = 6000; s.dsAmt = 1; s.dsThr = -35; s.lmOn = 0; s.dsFirst = (float)(f == 2);
            oc::Chain c; c.prepare(sr); c.setAutoLevel(false); c.set(s); std::vector<float> oL, oR; run(c, x, x, oL, oR);
            const int lat = c.latency(), per = (int)(0.34f * sr); double hs = 0, vs = 0;
            onek::Biquad hp5; hp5.highpass(sr, 5000.f); hp5.reset(); std::vector<float> hf(oL.size()); for (size_t i = 0; i < oL.size(); ++i) hf[i] = hp5.process(oL[i]);
            for (int ph = 1; ph < 6; ++ph) { for (int i = 0; i < (int)(0.09f * sr); ++i) { const double v = hf[(size_t)(ph * per + lat + i)]; hs += v * v; }
                for (int i = (int)(0.1f * sr); i < (int)(0.3f * sr); ++i) { const double v = oL[(size_t)(ph * per + lat + i)]; vs += v * v; } }
            sib[f] = 10 * std::log10(hs / 5 / (0.09 * sr) + 1e-12); vow[f] = 10 * std::log10(vs / 5 / (0.2 * sr) + 1e-12);
        }
        std::printf("de-ess avant comp : sibilante 5 kHz+ %.1f dB (sans de-ess) / %.1f dB (ordre d'origine) / %.1f dB (avant comp) ; voyelle %.1f / %.1f / %.1f dB\n", sib[0], sib[1], sib[2], vow[0], vow[1], vow[2]);
        CHECK(sib[2] < sib[0] - 1.0 && sib[1] < sib[0] - 1.0, "le de-esser n'agit plus dans un des deux ordres"); CHECK(std::fabs(sib[2] - sib[1]) > 0.5, "l'interrupteur d'ordre est sans effet");
        CHECK(std::fabs(vow[2] - vow[0]) < 1.0, "de-ess avant comp : la voyelle bouge de %.1f dB", vow[2] - vow[0]);
    }
    std::printf(fails ? "%d ECHEC(S)\n" : "TOUT EST BON\n", fails);
    return fails ? 1 : 0;
}
