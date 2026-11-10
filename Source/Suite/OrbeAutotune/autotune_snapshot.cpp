// Rend la face avant d'Orbe Autotune dans un PNG. Usage : orbeautotune_snapshot sortie.png [auto|manuel|recherche|edition] [clair|sombre|classique] [échelle]
// edition : capture réelle (tête de lecture simulée), découpage en notes, une note déplacée, vue ÉDITION.
#include "AutotuneProcessor.h"
#include "AutotuneEditor.h"
struct FakeHead : juce::AudioPlayHead {
    juce::int64 pos = 0;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setTimeInSamples(pos); p.setIsPlaying(true); return p; }
};
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    AutotuneProcessor proc;
    if (argc > 1 && juce::String(argv[1]) == "etat") {   // contrôle de l'état : ancien projet sans « algo » -> Classique ; notes éditées sauvegardées
        juce::MemoryBlock mb; proc.getStateInformation(mb);
        auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), (int)mb.getSize());
        for (auto* c = xml->getFirstChildElement(); c != nullptr;) { auto* nx = c->getNextElement(); const auto id = c->getStringAttribute("id"); if (id == "algo" || id == "expr" || id == "accid" || id == "rendu" || id == "glide") xml->removeChildElement(c, true); c = nx; }
        juce::MemoryBlock old; juce::AudioProcessor::copyXmlToBinary(*xml, old);
        AutotuneProcessor p2; p2.setStateInformation(old.getData(), (int)old.getSize());
        std::printf("ancien projet : algo = %s (attendu Classique)\n", p2.orbeOn() ? "Orbe" : "Classique");
        const int r2 = juce::roundToInt(p2.raw("rendu")), r1 = juce::roundToInt(proc.raw("rendu"));
        std::printf("rendu : ancien projet = %d (attendu 1 Precis), nouvelle instance = %d (attendu 0 Satin), doublage neuf = %.2f\n", r2, r1, proc.raw("dbl"));
        const bool rOk = r2 == 1 && r1 == 0 && proc.raw("dbl") < 0.001f;
        std::vector<ned::Note> n(2); n[0].start = 1000; n[0].end = 9000; n[0].center = 60.2f; n[0].shift = 80.f; n[0].edited = 1; n[1].start = 9000; n[1].end = 20000; n[1].center = 64.f;
        proc.setNotes(n, true); juce::MemoryBlock m2; proc.getStateInformation(m2);
        AutotuneProcessor p3; p3.setStateInformation(m2.getData(), (int)m2.getSize());
        const bool ok = p3.notes().size() == 2 && p3.notes()[0].shift == 80.f && p3.notes()[0].edited == 1 && p3.orbeOn();
        std::printf("notes editees rechargees : %s\n", ok ? "oui" : "NON");
        return (!p2.orbeOn() && ok && rOk) ? 0 : 1;
    }
    const double sr = 48000.0;
    proc.prepareToPlay(sr, 512);
    const juce::String mode = argc > 2 ? argv[2] : "manuel";
    auto set = [&](const char* id, float v) { if (auto* p = proc.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v)); };
    if (mode == "manuel") { set("key", 10); set("scale", 1); set("harm", 1); set("x6", 1); }   // A min (key 10 = A), note F# ajoutée
    if (mode == "recherche") set("key", 0);
    FakeHead head;
    if (mode == "edition") { set("key", 10); set("scale", 1); set("algo", 1); proc.setPlayHead(&head); proc.armCapture(); }
    const float mel[10] = {69, 72, 76, 74, 71, 67, 72, 69, 76, 72};
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
    double ph = 0; int note = 0, left = 0; float cur = 69.f;
    for (int blk = 0; blk < 560; ++blk) {
        for (int i = 0; i < 512; ++i) {
            if (--left <= 0) { cur = mel[note++ % 10] + (note % 3 - 1) * 0.28f; left = (int)(sr * 0.32); }
            const float vib = 0.18f * std::sin(0.00075f * (float)(blk * 512 + i) * 6.f);
            ph += 2 * 3.14159265358979 * 440.0 * std::pow(2.0, (cur + vib - 69.0) / 12.0) / sr;
            float v = 0; for (int k = 1; k <= 7; ++k) v += std::sin((float)(k * ph)) / (float)k;
            b.setSample(0, i, 0.22f * v); b.setSample(1, i, 0.22f * v);
        }
        head.pos = (juce::int64)blk * 512;
        proc.processBlock(b, m);
    }
    if (mode == "edition") {
        proc.stopCapture(); proc.pollCapture();
        auto n = proc.notes(); if (n.size() > 3) { n[2].shift += 100.f; n[2].edited = 1; n[3].shift = std::round((std::round(n[3].center) - n[3].center) * 100.f); n[3].edited = 1; }
        proc.setNotes(n, true);
        std::printf("edition : %d trames capturees, %d notes\n", (int)proc.curve().size(), (int)proc.notes().size());
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* ae = dynamic_cast<AutotuneEditor*>(ed.get());
    const juce::String th = argc > 3 ? argv[3] : "classique";   // sans argument : rendu d'origine (comparaisons existantes)
    ae->setThemeMode(th == "sombre" ? kit::v3::Mode::Dark : th == "clair" ? kit::v3::Mode::Light : kit::v3::Mode::Classic, false);
    ae->setUiScale(argc > 4 ? (float)std::atof(argv[4]) : 1.f);
    if (mode == "edition") ae->showEdition(true);
    for (int i = 0; i < 12; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::File f(argv[1]); f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
