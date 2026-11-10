// Orbe Sampler : colle JUCE partagée par Orbe Pads et Orbe Sample
// (échantillon + analyse en thread de fond, publication sans verrou vers le thread audio, état, composants d'interface).
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <mutex>
#include "../../Kit/Widgets.h"
#include "PadEngine.h"
#include "TransformEngine.h"

namespace osamp {

// Formats proposés au sélecteur et au glisser-déposer : seulement ceux que loadFile sait décoder sur cette plateforme.
// MP3 : décodeur JUCE (JUCE_USE_MP3AUDIOFORMAT=1 dans plugin.cmake) ; M4A et CAF : CoreAudio, donc Mac seulement.
#if JUCE_MAC || JUCE_IOS
inline const char* audioWildcards() { return "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a;*.caf"; }
inline const char* audioExtensions() { return "wav;aif;aiff;flac;ogg;mp3;m4a;caf"; }
#else
inline const char* audioWildcards() { return "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3"; }
inline const char* audioExtensions() { return "wav;aif;aiff;flac;ogg;mp3"; }
#endif

// Publication d'un objet immuable : le thread audio lit un pointeur brut, les autres threads gardent les propriétaires.
// Les anciens objets restent vivants 30 s, largement plus que n'importe quelle voix.
template <class T>
class Swap {
public:
    void publish(std::shared_ptr<const T> p) {
        std::lock_guard<std::mutex> l(m);
        const double now = juce::Time::getMillisecondCounterHiRes();
        raw.store(p.get(), std::memory_order_release);
        cur = p;
        hist.push_back({std::move(p), now});
        hist.erase(std::remove_if(hist.begin(), hist.end(), [&](const Item& i) { return now - i.t > 30000.0 && i.p != cur; }), hist.end());
    }
    const T* get() const { return raw.load(std::memory_order_acquire); }
    std::shared_ptr<const T> share() const { std::lock_guard<std::mutex> l(m); return cur; }
private:
    struct Item { std::shared_ptr<const T> p; double t; };
    mutable std::mutex m;
    std::atomic<const T*> raw{nullptr};
    std::shared_ptr<const T> cur;
    std::vector<Item> hist;
};

// L'échantillon source, son nom et le résultat de l'analyse (tempo, tonalité) calculée dans un thread de fond.
class SampleHolder : private juce::Thread {
public:
    SampleHolder();
    ~SampleHolder() override;
    void set(std::shared_ptr<const Sample> s, const juce::String& name);
    std::shared_ptr<const Sample> get() const { std::lock_guard<std::mutex> l(m); return smp; }
    juce::String name() const { std::lock_guard<std::mutex> l(m); return nm; }
    // Décode un fichier audio (WAV, AIFF, FLAC, OGG...) : stéréo, au plus maxSeconds secondes. Retourne nullptr en cas d'échec.
    static std::shared_ptr<Sample> loadFile(const juce::File& f, double maxSeconds, bool* truncated = nullptr);
    void writeTo(juce::MemoryOutputStream&) const;
    bool readFrom(juce::MemoryInputStream&);

    std::atomic<float> bpm{0.f}, bpmConf{0.f};
    std::atomic<int> pc{-1}, minor{0}, state{0};  // state : 0 vide, 1 analyse en cours, 2 analysé
    std::atomic<int> revision{0};
    std::function<void()> onAnalysed;             // appelé depuis le thread d'analyse
    double maxSeconds = 60.0;

private:
    void run() override;
    mutable std::mutex m;
    std::shared_ptr<const Sample> smp, todo;
    juce::String nm;
};

// État commun : XML des paramètres + échantillon embarqué.
void writeState(juce::MemoryOutputStream& mo, juce::AudioProcessorValueTreeState& apvts, const SampleHolder& h);
bool readState(juce::MemoryInputStream& mi, juce::AudioProcessorValueTreeState& apvts, SampleHolder& h);

inline juce::String fr(double v, int dec = 1) { return juce::String(v, dec).replace(".", ","); }
juce::String noteName(int midi);  // MIDI 36 = C1, lettres uniquement
juce::String keyName(int pc, bool minor);  // « A min », « C maj »
juce::String durText(double s);

// ---------------- composants d'interface ----------------

// Sélecteur à touches relié à un paramètre « choix » : kit commun.
using Seg = kit::w::Seg;

// Potard avec valeur lue dans un petit afficheur et légende gravée.
class Knob : public juce::Component, public juce::SettableTooltipClient {
public:
    Knob(juce::AudioProcessorValueTreeState& a, const juce::String& id, const juce::String& caption, const kit::Palette& pal);
    void bind(const juce::String& id);
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::Slider s;
    std::function<juce::String()> textOverride;  // valeur affichée à la place de celle du paramètre (si non vide)
private:
    juce::AudioProcessorValueTreeState& apvts;
    kit::Palette P;
    juce::String cap;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Forme d'onde dans un afficheur encastré, avec dessin et souris délégués à l'éditeur.
class WaveView : public juce::Component {
public:
    WaveView(const kit::Palette& pal, juce::Colour wave) : P(pal), col(wave) {}
    void setSample(std::shared_ptr<const Sample> s) { smp = std::move(s); rebuild(); repaint(); }
    const std::shared_ptr<const Sample>& sample() const { return smp; }
    juce::Rectangle<float> inner() const { return getLocalBounds().toFloat().reduced(juce::jmax(6.f, getHeight() * 0.04f)); }
    float xOf(double frame) const { auto r = inner(); return r.getX() + (smp && smp->size() ? (float)(frame / smp->size()) * r.getWidth() : 0.f); }
    double frameAt(float x) const { auto r = inner(); return smp ? juce::jlimit(0.0, 1.0, (double)((x - r.getX()) / r.getWidth())) * smp->size() : 0.0; }
    void setColour(juce::Colour c) { col = c; repaint(); }
    juce::String emptyText;
    std::function<void(juce::Graphics&, WaveView&)> paintOver;
    std::function<void(const juce::MouseEvent&)> onDown, onDrag, onUp, onDouble, onMove;
    void paint(juce::Graphics&) override;
    void resized() override { rebuild(); }
    void mouseDown(const juce::MouseEvent& e) override { if (onDown) onDown(e); }
    void mouseDrag(const juce::MouseEvent& e) override { if (onDrag) onDrag(e); }
    void mouseUp(const juce::MouseEvent& e) override { if (onUp) onUp(e); }
    void mouseMove(const juce::MouseEvent& e) override { if (onMove) onMove(e); }
    void mouseDoubleClick(const juce::MouseEvent& e) override { if (onDouble) onDouble(e); }
private:
    void rebuild();
    kit::Palette P;
    juce::Colour col;
    std::shared_ptr<const Sample> smp;
    std::vector<float> mn, mx;
};

// Mini forme d'onde d'une tranche (pour les pads).
std::vector<float> miniPeaks(const Sample& s, int a, int b, int cols);

// Titre gravé + sérigraphie de marque, commun aux deux éditeurs.
void paintBrand(juce::Graphics& g, const kit::Palette& p, juce::Rectangle<float> r, const juce::String& title, const juce::String& sub);

}  // namespace osamp
