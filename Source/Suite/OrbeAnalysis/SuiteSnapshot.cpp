// Rend l'éditeur d'un plugin de la suite dans un PNG (sans DAW), après avoir traité un vrai signal.
// Usage : orbe_suite_snapshot detect|mix|fredonne sortie.png signal.wav [largeur]
#include <juce_audio_utils/juce_audio_utils.h>
#include "../OrbeDetect/DetectProcessor.h"
#include "../OrbeMix/MixProcessor.h"
#include "../OrbeFredonne/FredonneProcessor.h"

static void pump(int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }

template <class P> static void feed(P& proc, const juce::AudioBuffer<float>& in) {
    juce::AudioBuffer<float> blk(2, 512); juce::MidiBuffer midi;
    for (int p = 0; p < in.getNumSamples(); p += 512) {
        const int n = juce::jmin(512, in.getNumSamples() - p); blk.setSize(2, n, false, false, true);
        for (int c = 0; c < 2; ++c) blk.copyFrom(c, 0, in, juce::jmin(c, in.getNumChannels() - 1), p, n);
        proc.processBlock(blk, midi);
    }
}

int main(int argc, char** argv) {
    if (argc < 4) return 2;
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String which = argv[1];
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(juce::File(argv[3])));
    if (!rd) return 3;
    juce::AudioBuffer<float> wav((int)juce::jmin<int64_t>(2, rd->numChannels), (int)rd->lengthInSamples);
    rd->read(&wav, 0, wav.getNumSamples(), 0, true, true);
    const double sr = rd->sampleRate;
    const int w = argc > 4 ? juce::String(argv[4]).getIntValue() : 1000;
    std::unique_ptr<juce::AudioProcessor> proc;
    if (which == "detect") {
        auto p = std::make_unique<DetectProcessor>(); p->prepareToPlay(sr, 512); p->hostBpm = 120.f; feed(*p, wav); p->analyze();
        for (int i = 0; i < 600 && p->busy(); ++i) pump(50);
        proc = std::move(p);
    } else if (which == "mix") {
        auto p = std::make_unique<MixProcessor>(); p->prepareToPlay(sr, 512); feed(*p, wav); feed(*p, wav);   // deux passes : plus de 3 s de matière
        proc = std::move(p);
    } else {
        auto p = std::make_unique<FredonneProcessor>(); p->prepareToPlay(sr, 512); p->pressRecord(); feed(*p, wav); p->pressRecord();
        for (int i = 0; i < 1200 && !(p->state() == FredonneProcessor::Idle && !p->take()->seg.notes.empty()); ++i) pump(50);
        printf("notes: %d\n", (int)p->take()->seg.notes.size());
        proc = std::move(p);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc->createEditor());
    ed->setSize(w, (int)(w * 0.64));
    for (int i = 0; i < 8; ++i) pump(40);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.5f);
    juce::File f(argv[2]); f.deleteFile(); juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    ed.reset(); proc.reset();
    return 0;
}
