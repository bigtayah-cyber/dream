// Tests d'Orbe FX sans DAW : audio traité (et non silencieux), NaN / inf, latence nulle, bypass, mix, mono, stabilité, préréglages, états.
// Usage : orbefx_test (code 0 = OK)   |   orbefx_test --snap sortie.png [largeur] [préréglage]
#include "OrbeFxProcessor.h"
#include "../../Kit/v3/InstrumentSkin.h"
#include <random>
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::printf("ECHEC: %s\n", msg); } } while (0)
static void setP(OrbeFxProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter(id); q->setValueNotifyingHost(q->convertTo0to1(v)); }
static float raw(OrbeFxProcessor& p, const char* id) { return p.apvts.getRawParameterValue(id)->load(); }

struct Sig { std::vector<float> l, r; };
static Sig noise(int n, float amp, unsigned seed = 1) {
    std::mt19937 g(seed); std::uniform_real_distribution<float> d(-1.f, 1.f);
    Sig s; s.l.resize((size_t)n); s.r.resize((size_t)n);
    for (int i = 0; i < n; ++i) { s.l[(size_t)i] = amp * d(g); s.r[(size_t)i] = amp * d(g); }
    return s;
}
static Sig sine(int n, float f, float amp, double sr) {
    Sig s; s.l.resize((size_t)n); s.r.resize((size_t)n);
    for (int i = 0; i < n; ++i) s.l[(size_t)i] = s.r[(size_t)i] = amp * std::sin(6.2831853 * f * i / sr);
    return s;
}
// Passe le signal par blocs ; renvoie la sortie.
static Sig run(OrbeFxProcessor& p, const Sig& in, int block = 512) {
    Sig out; out.l.resize(in.l.size()); out.r.resize(in.l.size());
    juce::AudioBuffer<float> b(2, block); juce::MidiBuffer m;
    for (size_t pos = 0; pos < in.l.size(); pos += (size_t)block) {
        const int n = (int)std::min<size_t>((size_t)block, in.l.size() - pos);
        b.setSize(2, n, false, false, true);
        std::copy(in.l.begin() + (long)pos, in.l.begin() + (long)pos + n, b.getWritePointer(0));
        std::copy(in.r.begin() + (long)pos, in.r.begin() + (long)pos + n, b.getWritePointer(1));
        p.processBlock(b, m);
        std::copy(b.getReadPointer(0), b.getReadPointer(0) + n, out.l.begin() + (long)pos);
        std::copy(b.getReadPointer(1), b.getReadPointer(1) + n, out.r.begin() + (long)pos);
    }
    return out;
}
static bool finite(const Sig& s, float lim = 1e3f) { for (size_t i = 0; i < s.l.size(); ++i) if (!std::isfinite(s.l[i]) || !std::isfinite(s.r[i]) || std::fabs(s.l[i]) > lim || std::fabs(s.r[i]) > lim) return false; return true; }
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b && i < v.size(); ++i) s += (double)v[i] * v[i]; return std::sqrt(s / (double)std::max<size_t>(1, b - a)); }
static float peak(const Sig& s) { float p = 0; for (size_t i = 0; i < s.l.size(); ++i) p = std::max(p, std::max(std::fabs(s.l[i]), std::fabs(s.r[i]))); return p; }
static void prep(OrbeFxProcessor& p, double sr = 48000.0, int bs = 512) { p.setPlayConfigDetails(2, 2, sr, bs); p.prepareToPlay(sr, bs); }

static int snap(int, char** argv, int argc) {
    kit::v3::snapshotTheme("orbefx");
    OrbeFxProcessor proc; prep(proc);
    if (argc > 4) proc.setCurrentProgram(juce::String(argv[4]).getIntValue());
    run(proc, sine(48000, 220.f, 0.4f, 48000.0));
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    const int w = argc > 3 ? juce::String(argv[3]).getIntValue() : 1100;
    ed->setSize(w, (int)(w * 700.0 / 1100.0));
    for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[2]); f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String(argv[1]) == "--snap") return snap(argc, argv, argc);
    const int N = 48000 * 3;
    // 1. réglages d'usine : traversée quasi transparente, latence nulle, rien de silencieux
    {
        OrbeFxProcessor p; prep(p);
        CHECK(p.getLatencySamples() == 0, "latence non nulle");
        auto in = sine(N, 440.f, 0.5f, 48000.0); auto out = run(p, in);
        double err = 0; for (size_t i = 20000; i < in.l.size(); ++i) err = std::max(err, (double)std::fabs(out.l[i] - in.l[i]));
        CHECK(err < 0.02, "réglages d'usine : sortie trop différente de l'entrée");
        std::printf("[1] usine : écart max %.5f\n", err);
        auto imp = Sig{std::vector<float>(2048, 0.f), std::vector<float>(2048, 0.f)}; imp.l[0] = imp.r[0] = 1.f;
        OrbeFxProcessor q; prep(q); setP(q, "fcut", 2000.f); auto o = run(q, imp);
        CHECK(std::fabs(o.l[0]) > 0.05f, "impulsion absente du premier échantillon (latence ?)");
    }
    // 2. l'audio est vraiment traité : filtre passe-bas, saturation, delay, réverbe
    {
        OrbeFxProcessor p; prep(p); setP(p, "fcut", 300.f);
        auto in = noise(N, 0.4f); auto out = run(p, in);
        CHECK(rms(out.l, 24000, (size_t)N) < 0.35 * rms(in.l, 24000, (size_t)N), "passe-bas 300 Hz : le bruit n'est pas atténué");
        CHECK(rms(out.l, 24000, (size_t)N) > 1e-4, "passe-bas : sortie muette");
        OrbeFxProcessor s; prep(s); setP(s, "sdrive", 24.f); setP(s, "smix", 1.f);
        auto sin = sine(N, 200.f, 0.3f, 48000.0); auto so = run(s, sin);
        CHECK(peak(so) > 0.05f && finite(so, 4.f), "saturation : muette ou instable");
        double diff = 0; for (size_t i = 24000; i < sin.l.size(); ++i) diff += std::fabs(so.l[i] - sin.l[i]); CHECK(diff > 100.0, "saturation sans effet");
        OrbeFxProcessor d; prep(d); setP(d, "dmix", 1.f); setP(d, "dtime", 3.f);
        Sig imp{std::vector<float>(N, 0.f), std::vector<float>(N, 0.f)}; imp.l[100] = imp.r[100] = 0.8f;
        auto dout = run(d, imp);
        CHECK(std::fabs(dout.l[100 + 24000]) > 0.05f || std::fabs(dout.r[100 + 24000]) > 0.05f, "delay : pas d'écho à 1/4 de noire à 120 BPM (24000 échantillons)");
        OrbeFxProcessor rv; prep(rv); setP(rv, "rmix", 1.f); setP(rv, "rdecay", 2.5f);
        auto ro = run(rv, imp);
        const double e1 = rms(ro.l, 24000, 48000), e2 = rms(ro.l, 120000, 144000);
        CHECK(e1 > 1e-5 && e2 < e1 * 0.5, "réverbe : queue absente ou qui ne décroît pas");
        std::printf("[2] traitement : reverb %.5f -> %.6f\n", e1, e2);
    }
    // 3. NaN / inf / énorme en entrée
    {
        OrbeFxProcessor p; prep(p); setP(p, "rmix", 0.5f); setP(p, "dmix", 0.5f); setP(p, "cmix", 0.5f); setP(p, "smix", 0.5f);
        auto in = noise(48000, 0.3f); in.l[10] = NAN; in.r[20] = INFINITY; in.l[30] = -INFINITY; in.r[40] = 1e30f;
        auto out = run(p, in); auto tail = run(p, noise(96000, 0.3f, 7));
        CHECK(finite(out, 50.f) && finite(tail, 50.f), "NaN / inf en entrée : sortie non finie ou énorme");
    }
    // 4. bypass : entrée telle quelle après le fondu ; mix 0 = sec
    {
        OrbeFxProcessor p; prep(p); p.setCurrentProgram(3);
        setP(p, "bypass", 1.f);
        auto in = noise(N, 0.4f); auto out = run(p, in);
        double err = 0; for (size_t i = 24000; i < in.l.size(); ++i) err = std::max(err, (double)std::fabs(out.l[i] - in.l[i]));
        CHECK(err < 1e-6, "bypass : l'entrée ne traverse pas à l'identique");
        OrbeFxProcessor q; prep(q); q.setCurrentProgram(10); setP(q, "mix", 0.f);
        auto o2 = run(q, in); err = 0; for (size_t i = 24000; i < in.l.size(); ++i) err = std::max(err, (double)std::fabs(o2.l[i] - in.l[i]));
        CHECK(err < 1e-4, "mix 0 : sortie différente du sec");
        std::printf("[4] bypass et mix OK\n");
    }
    // 5. entrée mono
    {
        OrbeFxProcessor p; p.setPlayConfigDetails(1, 2, 48000.0, 512); p.prepareToPlay(48000.0, 512);
        juce::AudioProcessor::BusesLayout l; l.inputBuses.add(juce::AudioChannelSet::mono()); l.outputBuses.add(juce::AudioChannelSet::stereo());
        CHECK(p.isBusesLayoutSupported(l), "mono -> stéréo refusé");
        setP(p, "dmix", 0.3f);
        auto out = run(p, sine(48000, 330.f, 0.4f, 48000.0));
        CHECK(peak(out) > 0.1f && finite(out), "entrée mono : sortie muette ou non finie");
    }
    // 6. tous les préréglages : non muets, finis, bornés ; 7. extrêmes ; 8. fréquences d'échantillonnage
    {
        for (int i = 0; i < OrbeFxProcessor::numPresets(); ++i) {
            OrbeFxProcessor p; prep(p); p.setCurrentProgram(i);
            CHECK(p.presetMatches(i), "préréglage non reconnu après application");
            auto out = run(p, sine(N, 220.f, 0.4f, 48000.0));
            CHECK(peak(out) > 0.02f && finite(out, 8.f), (std::string("préréglage muet / instable : ") + p.getProgramName(i).toStdString()).c_str());
        }
        OrbeFxProcessor p; prep(p);
        for (auto id : {"dfb", "rsize", "mcol", "mgrain", "mmove", "mspace", "fres", "stone", "dtone", "rdamp", "cdepth"}) setP(p, id, 1.f);
        setP(p, "rdecay", 12.f); setP(p, "sdrive", 30.f); setP(p, "dmix", 1.f); setP(p, "rmix", 1.f); setP(p, "cmix", 1.f); setP(p, "smix", 1.f); setP(p, "fcut", 800.f);
        auto out = run(p, noise(48000 * 12, 0.9f, 3), 256);
        CHECK(finite(out, 16.f), "extrêmes : instable");
        auto rest = run(p, Sig{std::vector<float>(48000 * 4, 0.f), std::vector<float>(48000 * 4, 0.f)});
        CHECK(finite(rest, 16.f), "extrêmes : la queue explose");
        for (double sr : {22050.0, 44100.0, 88200.0, 96000.0, 192000.0}) {
            OrbeFxProcessor q; prep(q, sr, 1024); q.setCurrentProgram(5);
            auto o = run(q, sine((int)sr, 330.f, 0.4f, sr), 1024);
            CHECK(finite(o, 8.f) && peak(o) > 0.02f, "fréquence d'échantillonnage : sortie muette ou instable");
        }
        for (int bs : {1, 7, 64, 4096}) { OrbeFxProcessor q; prep(q, 48000.0, bs); q.setCurrentProgram(7); auto o = run(q, noise(20000, 0.3f), bs); CHECK(finite(o, 8.f), "taille de bloc : instable"); }
        std::printf("[6-8] préréglages, extrêmes, fréquences, blocs OK\n");
    }
    // 9. états : aller-retour ; ancien état « ORBE » ignoré ; état invalide
    {
        OrbeFxProcessor a; setP(a, "fcut", 1234.f); setP(a, "dtime", 4.f); setP(a, "bypass", 1.f); a.setCurrentProgram(6); setP(a, "mcol", -0.4f); setP(a, "rmix", 0.77f);
        juce::MemoryBlock mb; a.getStateInformation(mb);
        OrbeFxProcessor b; b.setStateInformation(mb.getData(), (int)mb.getSize());
        CHECK(std::fabs(raw(b, "rmix") - 0.77f) < 1e-3f && raw(b, "bypass") > 0.5f && std::fabs(raw(b, "mcol") + 0.4f) < 1e-3f && juce::roundToInt(raw(b, "dtime")) == (juce::roundToInt(raw(a, "dtime"))), "état : aller-retour incomplet");
        CHECK(b.getCurrentProgram() == 6, "état : préréglage courant perdu");
        juce::XmlElement old("ORBE"); old.setAttribute("style", 3); old.setAttribute("key", 5);
        juce::MemoryBlock ob; juce::AudioProcessor::copyXmlToBinary(old, ob);
        OrbeFxProcessor c; setP(c, "fcut", 500.f); c.setStateInformation(ob.getData(), (int)ob.getSize());
        CHECK(std::fabs(raw(c, "fcut") - 500.f) < 1.f, "ancien état Orbe : doit être ignoré");
        const char junk[] = "pas un état"; c.setStateInformation(junk, (int)sizeof(junk)); c.setStateInformation(nullptr, 0);
        CHECK(std::fabs(raw(c, "fcut") - 500.f) < 1.f, "état invalide : réglages perdus");
        CHECK(c.getTailLengthSeconds() >= 0.0, "queue négative");
        std::printf("[9] états OK\n");
    }
    // 10. éditeur : construit, dessine, se détruit pour chaque thème
    {
        OrbeFxProcessor p; prep(p);
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        ed->setSize(1100, 700); for (int i = 0; i < 3; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        CHECK(img.isValid(), "éditeur non dessiné");
    }
    std::printf(fails == 0 ? "orbefx_test : tout est OK\n" : "orbefx_test : %d ECHEC(S)\n", fails);
    return fails == 0 ? 0 : 1;
}
