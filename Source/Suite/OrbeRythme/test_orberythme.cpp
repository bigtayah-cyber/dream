// Tests d'Orbe Rythme sans DAW : migration depuis un ancien état d'Orbe, état propre, édition, audio, MIDI, Orbe sans batterie.
// Usage : orberythme_test (code 0 = OK)
#include "OrbeRythmeProcessor.h"
#include "../../Orbe/OrbeProcessor.h"
#include "../../Kit/v3/InstrumentSkin.h"
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::printf("ECHEC: %s\n", msg); } } while (0)

static void pump(int ms = 120) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }
template <class P> static void setP(P& p, const char* id, float v) { auto* q = p.apvts.getParameter(id); q->setValueNotifyingHost(q->convertTo0to1(v)); }
static bool same(const std::vector<orbe::Note>& a, const std::vector<orbe::Note>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (a[i].t != b[i].t || a[i].n != b[i].n || std::fabs(a[i].v - b[i].v) > 1e-3f || a[i].d != b[i].d) return false;
    return true;
}
static float raw(juce::AudioProcessorValueTreeState& a, const char* id) { return a.getRawParameterValue(id)->load(); }

// Contrôle visuel : orberythme_test --snap rythme|orbe sortie.png [largeur] [style] [8]   (DSG_THEME=clair|sombre|classique)
static int snap(int argc, char** argv) {
    const bool orbe = juce::String(argv[2]) == "orbe";
    kit::v3::snapshotTheme(orbe ? "orbe" : "orberythme");
    std::unique_ptr<juce::AudioProcessor> proc;
    if (orbe) proc = std::make_unique<OrbeProcessor>(); else proc = std::make_unique<OrbeRythmeProcessor>();
    auto setP = [&](const char* id, float v) { if (auto* q = (orbe ? static_cast<OrbeProcessor*>(proc.get())->apvts : static_cast<OrbeRythmeProcessor*>(proc.get())->apvts).getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
    proc->prepareToPlay(48000, 512);
    if (argc > 5) setP("style", (float)juce::String(argv[5]).getIntValue());
    if (argc > 6) setP("bars", 1.f);
    for (int i = 0; i < 4; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    if (orbe) static_cast<OrbeProcessor*>(proc.get())->audition = true; else static_cast<OrbeRythmeProcessor*>(proc.get())->audition = true;
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    for (int i = 0; i < 60; ++i) { m.clear(); proc->processBlock(b, m); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc->createEditor());
    const int w = argc > 4 ? juce::String(argv[4]).getIntValue() : 1000;
    ed->setSize(w, (int)(w * (orbe ? 640.0 : 580.0) / 1000.0));
    for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[3]); f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 3 && juce::String(argv[1]) == "--snap") return snap(argc, argv);
    // 1. migration : l'état d'un ancien Orbe redonne la même batterie (styles et longueurs variés, réglages harmoniques qui changent le tirage)
    {
        int ok = 0, total = 0;
        for (int style : {0, 1, 3, 8, 12, 16, 20, 24, 27})
            for (int bars : {0, 1}) {
                OrbeProcessor a;
                setP(a, "style", (float)style); setP(a, "bars", (float)bars); setP(a, "key", 3.f); setP(a, "borrow", (style % 2) ? 1.f : 0.f);
                setP(a, "human", 0.3f); setP(a, "vol3", 0.6f); setP(a, "mute3", (style % 3 == 0) ? 1.f : 0.f); setP(a, "master", 0.5f); setP(a, "arp", (float)(style % 4));
                pump();
                a.generate(OrbeProcessor::Gen::All);
                juce::MemoryBlock mb; a.getStateInformation(mb);
                OrbeRythmeProcessor r;
                r.setStateInformation(mb.getData(), (int)mb.getSize());
                ++total;
                const bool drums = same(a.song().tr(3), r.notes());
                const bool params = (int)raw(r.apvts, "style") == style && (int)raw(r.apvts, "bars") == bars && std::fabs(raw(r.apvts, "level") - 0.6f) < 1e-3f
                                 && (int)raw(r.apvts, "mute") == (style % 3 == 0 ? 1 : 0) && std::fabs(raw(r.apvts, "human") - 0.3f) < 1e-3f && std::fabs(raw(r.apvts, "master") - 0.5f) < 1e-3f;
                CHECK(drums, "migration : batterie differente de celle de l'ancien Orbe");
                CHECK(params, "migration : parametres style / mesures / niveau / muet non repris");
                CHECK(!r.notes().empty() && r.bars() == (bars ? 8 : 4), "migration : batterie vide ou mauvaise longueur");
                ok += drums && params;
            }
        std::printf("[1] migration ancien Orbe -> Rythme : %d / %d identiques\n", ok, total);
    }
    // 2. migration : piste batterie éditée ou verrouillée dans l'ancien Orbe = frappes restaurées telles quelles
    {
        OrbeProcessor a; setP(a, "style", 1.f); pump();
        a.beginEdit(); a.addNote(3, 5.f, 0.5f, 38); a.removeNote(3, 0);
        juce::MemoryBlock mb; a.getStateInformation(mb);
        OrbeRythmeProcessor r; r.setStateInformation(mb.getData(), (int)mb.getSize());
        CHECK(same(a.song().tr(3), r.notes()), "migration : frappes editees non restaurees");
        CHECK(r.edited(), "migration : drapeau modifie absent");
        OrbeProcessor b; setP(b, "style", 5.f); pump(); b.toggleTrackLock(3);
        juce::MemoryBlock mb2; b.getStateInformation(mb2);
        OrbeRythmeProcessor r2; r2.setStateInformation(mb2.getData(), (int)mb2.getSize());
        CHECK(same(b.song().tr(3), r2.notes()), "migration : batterie verrouillee non restauree");
        std::printf("[2] edition / verrou migres : %zu et %zu frappes\n", r.notes().size(), r2.notes().size());
    }
    // 3. état propre : aller-retour, y compris frappes modifiées et contexte harmonique
    {
        OrbeRythmeProcessor a; setP(a, "style", 4.f); setP(a, "swing", 0.4f); pump();
        a.generate();
        a.beginEdit(); a.toggleHit(4, 6); a.toggleHit(0, 3);
        juce::MemoryBlock mb; a.getStateInformation(mb);
        OrbeRythmeProcessor b; b.setStateInformation(mb.getData(), (int)mb.getSize());
        CHECK(same(a.notes(), b.notes()), "etat propre : frappes differentes apres restauration");
        CHECK(std::fabs(raw(b.apvts, "swing") - 0.4f) < 1e-3f && (int)raw(b.apvts, "style") == 4, "etat propre : parametres non restaures");
        CHECK(b.edited(), "etat propre : drapeau modifie perdu");
        // état sans modification : régénéré à l'identique depuis la graine
        OrbeRythmeProcessor c; setP(c, "style", 9.f); pump(); c.generate();
        juce::MemoryBlock m3; c.getStateInformation(m3);
        OrbeRythmeProcessor d; d.setStateInformation(m3.getData(), (int)m3.getSize());
        CHECK(same(c.notes(), d.notes()) && !d.edited(), "etat propre : tirage non reproductible depuis la graine");
        std::printf("[3] etat propre : %zu frappes restaurees\n", b.notes().size());
    }
    // 4. édition : poser / retirer / annuler / vélocité
    {
        OrbeRythmeProcessor a; pump();
        const size_t n0 = a.notes().size();
        const int row = 3, step = 13;
        const bool had = a.findHit(row, (float)step) >= 0;
        a.beginEdit(); a.toggleHit(row, step);
        CHECK(a.notes().size() == (had ? n0 - 1 : n0 + 1), "toggleHit : nombre de frappes");
        CHECK((a.findHit(row, (float)step) >= 0) != had, "toggleHit : frappe absente / presente");
        a.beginEdit(); a.toggleHit(row, step);
        CHECK(a.notes().size() == n0, "toggleHit deux fois : retour a l'etat initial");
        CHECK(a.undo(), "annulation possible");
        CHECK(a.redo(), "retablissement possible");
        a.beginEdit(); a.toggleHit(6, 0); const int i = a.findHit(6, 0.f);
        if (i >= 0) { const float v0 = a.notes()[(size_t)i].v; a.beginEdit(); a.cycleVelocity(6, 0); CHECK(std::fabs(a.notes()[(size_t)a.findHit(6, 0.f)].v - v0) > 1e-3f, "cycleVelocity : velocite inchangee"); }
        a.toggleHit(7, 0); a.toggleHit(0, -1); a.toggleHit(0, 9999);   // hors limites : ignorés sans planter
        a.generate(); CHECK(!a.edited(), "generer remplace les frappes modifiees");
        std::printf("[4] edition OK (%zu frappes)\n", a.notes().size());
    }
    // 5. audio : aperçu non silencieux, pas de NaN, MIDI canal 10 cohérent, rien ne reste tenu à l'arrêt
    {
        OrbeRythmeProcessor a; a.prepareToPlay(48000, 512); pump();
        setP(a, "midiout", 1.f); a.audition = true;
        juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi;
        double energy = 0; float peak = 0; bool bad = false; int on = 0, off = 0, wrongCh = 0; float maxStep = 0;
        for (int b = 0; b < 48000 * 8 / 512; ++b) {
            midi.clear(); buf.clear(); a.processBlock(buf, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { const float x = buf.getSample(c, i); if (!std::isfinite(x)) bad = true; energy += (double)x * x; peak = std::max(peak, std::fabs(x)); }
            for (auto m : midi) { const auto mm = m.getMessage(); if (mm.isNoteOn()) { ++on; wrongCh += mm.getChannel() != 10; } if (mm.isNoteOff()) ++off; }
            maxStep = std::max(maxStep, a.playStep.load());
        }
        a.audition = false;
        for (int b = 0; b < 300; ++b) { midi.clear(); buf.clear(); a.processBlock(buf, midi); for (auto m : midi) if (m.getMessage().isNoteOff()) ++off; }   // laisse finir les voix et les notes-off
        std::printf("[5] energie %.1f pic %.3f noteOn %d noteOff %d pas max %.1f\n", energy, peak, on, off, maxStep);
        CHECK(!bad, "NaN/inf dans la sortie"); CHECK(energy > 10.0, "sortie quasi silencieuse"); CHECK(peak <= 1.0f, "pic > 1");
        CHECK(on > 30 && on == off, "notes MIDI incoherentes ou tenues"); CHECK(wrongCh == 0, "note hors canal 10"); CHECK(maxStep > 20.f, "tete de lecture immobile");
        midi.clear(); buf.clear(); a.processBlock(buf, midi);
        CHECK(buf.getMagnitude(0, 512) < 1e-3f && a.playStep.load() < 0.f, "bruit residuel ou lecture non arretee");
        // muet : ni son ni MIDI
        setP(a, "mute", 1.f); a.audition = true; on = 0; energy = 0;
        for (int b = 0; b < 400; ++b) { midi.clear(); buf.clear(); a.processBlock(buf, midi); for (int c = 0; c < 2; ++c) energy += buf.getMagnitude(c, 0, 512); for (auto m : midi) on += m.getMessage().isNoteOn(); }
        CHECK(on == 0 && energy < 1e-6, "muet : son ou MIDI encore envoyes");
        a.audition = false;
    }
    // 6. Orbe (sans batterie) : accords, lead et basse présents, plus rien sur le canal 10, état de batterie intact
    {
        OrbeProcessor a; a.prepareToPlay(48000, 512); pump();
        setP(a, "midiout", 1.f); a.audition = true;
        juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi;
        int ch[17] = {}; double energy = 0; bool bad = false;
        for (int b = 0; b < 48000 * 8 / 512; ++b) {
            midi.clear(); buf.clear(); a.processBlock(buf, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { const float x = buf.getSample(c, i); if (!std::isfinite(x)) bad = true; energy += (double)x * x; }
            for (auto m : midi) if (m.getMessage().isNoteOn()) ++ch[m.getMessage().getChannel()];
        }
        std::printf("[6] Orbe : canaux 1/2/3/10 = %d/%d/%d/%d, energie %.1f\n", ch[1], ch[2], ch[3], ch[10], energy);
        CHECK(ch[1] > 0 && ch[2] > 0 && ch[3] > 0, "Orbe : accords, lead ou basse absents du MIDI");
        CHECK(ch[10] == 0, "Orbe joue encore la batterie");
        CHECK(!bad && energy > 50.0, "Orbe : audio nul ou NaN");
        CHECK(!a.song().tr(3).empty(), "Orbe : le moteur doit garder la piste 3 (etat compatible)");
        juce::MemoryBlock mb; a.getStateInformation(mb);
        OrbeProcessor b; b.setStateInformation(mb.getData(), (int)mb.getSize());
        CHECK(a.song().prog == b.song().prog && same(a.song().tr(1), b.song().tr(1)) && same(a.song().tr(3), b.song().tr(3)), "Orbe : etat non restaure");
    }
    // 7. entrées invalides : octets au hasard, mauvaise balise, état vide : aucune exception, état inchangé
    {
        OrbeRythmeProcessor a; pump();
        const auto before = a.notes();
        const char junk[] = "pas du xml";
        a.setStateInformation(junk, (int)sizeof junk);
        a.setStateInformation(junk, 0);
        juce::XmlElement other("AUTRE"); juce::MemoryBlock mb; juce::AudioProcessor::copyXmlToBinary(other, mb);
        a.setStateInformation(mb.getData(), (int)mb.getSize());
        CHECK(same(before, a.notes()), "etat invalide : la batterie a change");
        // un état d'Orbe FX (ancienne version = état d'Orbe) ou d'Orbe seul sans batterie éditée se relit sans erreur
        std::printf("[7] entrees invalides ignorees\n");
    }
    std::printf(fails ? "ECHECS : %d\n" : "orberythme_test : tout est OK\n", fails);
    return fails ? 1 : 0;
}
