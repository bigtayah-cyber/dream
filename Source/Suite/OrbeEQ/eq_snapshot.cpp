// Rend la face d'Orbe EQ dans un PNG et vérifie l'interaction souris. Usage : orbeeq_snapshot sortie.png [preset 0-8] [voix.wav]
#include "EqProcessor.h"
#include "EqEditor.h"
#include "../OrbeDsp/snapshot_util.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } } while (0)

static juce::MouseEvent ev(juce::Component& c, juce::Point<float> p, bool popup = false, int clicks = 1, bool shift = false) {
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    juce::ModifierKeys m(juce::ModifierKeys::leftButtonModifier | (shift ? juce::ModifierKeys::shiftModifier : 0));
    (void)popup;
    return juce::MouseEvent(src, p, m, 1.f, 0.f, 0.f, 0.f, 0.f, &c, &c, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), clicks, false);
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    EqProcessor proc; proc.prepareToPlay(48000, 512);
    proc.applyPreset(argc > 2 ? juce::String(argv[2]).getIntValue() : 1);
    auto v = argc > 3 ? snap::loadMono(argv[3], 48000) : std::vector<float>();
    if (v.size() < 48000 * 2) { v = snap::noise(48000 * 2, -30.f, 3); float lp = 0; for (auto& s : v) { lp += 0.08f * (s - lp); s = lp * 5.f; } }
    v.resize(48000 * 2);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(900, 548);
    snap::feed(proc, v); snap::settle(300); snap::feed(proc, v); snap::settle(200);

    // Interaction : glisser le point de la bande 5 (cloche 3,2 kHz), molette, double-clic, clic sur une pastille
    auto* graph = ed->getChildComponent(0);
    const float u = (float)graph->getWidth() / 836.f;
    const auto plot = graph->getLocalBounds().toFloat().withTrimmedLeft(36 * u).withTrimmedRight(12 * u).withTrimmedTop(12 * u).withTrimmedBottom(22 * u);
    auto px = [&](float f) { return plot.getX() + plot.getWidth() * std::log(f / 20.f) / std::log(1000.f); };
    auto py = [&](float db) { return plot.getBottom() - plot.getHeight() * (db + 12.f) / 24.f; };
    const auto before = proc.readParams();
    const int b = 4; juce::Point<float> p0(px(before.b[b].freq), py(before.b[b].gain));
    graph->mouseMove(ev(*graph, p0));
    graph->mouseDown(ev(*graph, p0));
    graph->mouseDrag(ev(*graph, {px(5000.f), py(-4.f)}));
    graph->mouseUp(ev(*graph, {px(5000.f), py(-4.f)}));
    auto after = proc.readParams();
    std::printf("glisser : bande 5 %.0f Hz %+.1f dB -> %.0f Hz %+.1f dB\n", before.b[b].freq, before.b[b].gain, after.b[b].freq, after.b[b].gain);
    CHECK(std::fabs(after.b[b].freq - 5000.f) < 60.f && std::fabs(after.b[b].gain + 4.f) < 0.3f, "glisser incorrect");
    const float q0 = after.b[b].q; juce::MouseWheelDetails w{}; w.deltaY = 0.25f;
    graph->mouseWheelMove(ev(*graph, {px(5000.f), py(-4.f)}), w);
    CHECK(proc.readParams().b[b].q > q0 * 1.2f, "molette : Q inchangé");
    graph->mouseDoubleClick(ev(*graph, {px(5000.f), py(-4.f)}, false, 2));
    CHECK(std::fabs(proc.readParams().b[b].gain) < 0.01f && std::fabs(proc.readParams().b[b].freq - eq::defaultBand(b).freq) < 2.f, "double-clic : pas de réinitialisation");
    graph->mouseDoubleClick(ev(*graph, {px(700.f), py(5.f)}, false, 2));  // zone vide : ajoute une bande libre
    std::printf("bande ajoutée par double-clic : bande 3 %s\n", proc.readParams().b[2].on ? "active" : "inactive");
    proc.applyPreset(argc > 2 ? juce::String(argv[2]).getIntValue() : 1);
    snap::settle(200);
    auto* ned = static_cast<juce::Component*>(ed.get());
    (void)ned;
    const bool ok = snap::savePng(*ed, argv[1]);
    std::printf(fails ? "%d ECHEC(S)\n" : "INTERACTION OK\n", fails);
    return ok && !fails ? 0 : 1;
}
