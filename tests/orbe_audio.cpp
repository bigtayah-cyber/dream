// Test du plugin Orbe sans DAW : lecture, MIDI, état. Usage : orbe_audiotest (code 0 = OK)
#include "../Source/Orbe/OrbeProcessor.h"
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::printf("ECHEC: %s\n", msg); } } while (0)
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    OrbeProcessor a;
    a.prepareToPlay(48000, 512);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(60);
    // 1. lecture interne : 8 s de boucle, son non nul, pas de NaN, pic borné
    {
        a.audition = true;
        if (auto* p = a.apvts.getParameter("midiout")) p->setValueNotifyingHost(1.f);
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        double energy = 0; float peak = 0; bool bad = false; int on = 0, off = 0; float maxStep = 0;
        for (int b = 0; b < 48000 * 8 / 512; ++b) {
            midi.clear(); buf.clear();
            a.processBlock(buf, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { const float x = buf.getSample(c, i); if (!std::isfinite(x)) bad = true; energy += x * x; peak = std::max(peak, std::fabs(x)); }
            for (auto m : midi) { if (m.getMessage().isNoteOn()) ++on; if (m.getMessage().isNoteOff()) ++off; }
            maxStep = std::max(maxStep, a.playStep.load());
        }
        std::printf("[1] energie %.1f pic %.3f noteOn %d noteOff %d pas max %.1f\n", energy, peak, on, off, maxStep);
        CHECK(!bad, "NaN/inf dans la sortie");
        CHECK(energy > 50.0, "sortie quasi silencieuse");
        CHECK(peak <= 1.0f, "pic > 1");
        CHECK(on > 50 && std::abs(on - off) <= 40, "notes MIDI incoherentes");
        CHECK(maxStep > 20.f, "tete de lecture n'avance pas");
        a.audition = false;
        for (int b = 0; b < 200; ++b) { midi.clear(); a.processBlock(buf, midi); }   // laisse finir les notes
    }
    // 2. arrêt : silence au bout d'un moment
    {
        juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi; buf.clear();
        a.processBlock(buf, midi);
        CHECK(buf.getMagnitude(0, 512) < 1e-3f, "bruit residuel apres arret");
        CHECK(a.playStep.load() < 0.f, "playStep devrait etre -1 a l'arret");
    }
    // 3. état : sauvegarde / restauration => même progression et mêmes paramètres
    {
        a.generate(OrbeProcessor::Gen::All);
        a.toggleLock(1);
        if (auto* p = a.apvts.getParameter("key")) p->setValueNotifyingHost(p->convertTo0to1(3.f));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
        juce::MemoryBlock mb; a.getStateInformation(mb);
        OrbeProcessor b;
        b.setStateInformation(mb.getData(), (int)mb.getSize());
        const auto &sa = a.song(), &sb = b.song();
        CHECK(sa.prog == sb.prog, "progression non restauree");
        CHECK(sa.nLead.size() == sb.nLead.size(), "lead different apres restauration");
        bool same = sa.nLead.size() == sb.nLead.size();
        for (size_t i = 0; same && i < sa.nLead.size(); ++i) same = sa.nLead[i].n == sb.nLead[i].n && sa.nLead[i].t == sb.nLead[i].t;
        CHECK(same, "notes de lead differentes apres restauration");
        CHECK(b.isLocked(1), "verrou non restaure");
        CHECK((int)b.apvts.getRawParameterValue("key")->load() == 3, "tonique non restauree");
        std::printf("[3] etat restaure : %zu accords, %zu notes de lead\n", sb.prog.size(), sb.nLead.size());
    }
    // 4. changement de style : gamme et 7e suivent, la progression a la bonne longueur
    {
        if (auto* p = a.apvts.getParameter("style")) p->setValueNotifyingHost(p->convertTo0to1(2.f));  // boom bap : dorien, 7e
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
        CHECK((int)a.apvts.getRawParameterValue("scale")->load() == 2, "gamme du style non appliquee");
        CHECK(a.apvts.getRawParameterValue("seventh")->load() > 0.5f, "7e du style non appliquee");
        CHECK(a.song().prog.size() == 4 && !a.song().nLead.empty(), "generation apres changement de style");
        auto f = a.writeMidiFile(true, true, true, true);
        CHECK(f.existsAsFile() && f.getSize() > 100, "fichier MIDI non ecrit");
        std::printf("[4] style change, MIDI ecrit : %s (%lld octets)\n", f.getFileName().toRawUTF8(), (long long)f.getSize());
    }
    // 5. édition : notes modifiées sauvegardées dans l'état, reflétées dans le MIDI écrit, annuler les restaure
    {
        OrbeProcessor e;
        e.prepareToPlay(48000, 512);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(60);
        auto key = [](const OrbeProcessor& p, int tr) { return orbe::Generator::pack(p.song().tr(tr)); };
        const std::string lead0 = key(e, 1), bass0 = key(e, 2);
        e.beginEdit(); e.addNote(1, 4.f, 2.f, 77); e.removeNote(2, 0);
        e.setChord(1, 1, 4);
        const std::string lead1 = key(e, 1), bass1 = key(e, 2);
        CHECK(lead1 != lead0 && bass1 != bass0, "edition sans effet");
        juce::MemoryBlock mb; e.getStateInformation(mb);
        OrbeProcessor f; f.setStateInformation(mb.getData(), (int)mb.getSize());
        CHECK(key(f, 1) == lead1 && key(f, 2) == bass1, "notes modifiees non restaurees par l'etat");
        CHECK(f.song().prog2 == e.song().prog2 && f.song().split == e.song().split, "2e accord non restaure");
        e.toggleTrackLock(1);
        e.generate(OrbeProcessor::Gen::All);
        CHECK(key(e, 1) == lead1, "piste lead verrouillee modifiee par Generer");
        juce::MemoryBlock mb2; e.getStateInformation(mb2);
        OrbeProcessor g2; g2.setStateInformation(mb2.getData(), (int)mb2.getSize());
        CHECK(g2.trackLocked(1) && key(g2, 1) == lead1, "verrou de piste non restaure");
        while (e.undo()) {}
        CHECK(key(e, 1) == lead0 && key(e, 2) == bass0 && !e.canUndo() && e.canRedo(), "annuler ne restaure pas l'etat initial");
        auto fl = e.writeMidiFile(true, true, true, true);
        CHECK(fl.existsAsFile(), "MIDI non ecrit apres annulation");
        std::printf("[5] edition / etat / annuler / verrous OK\n");
    }
    std::printf(fails ? "\n%d ECHEC(S)\n" : "\nTEST AUDIO ORBE OK\n", fails);
    return fails ? 1 : 0;
}
