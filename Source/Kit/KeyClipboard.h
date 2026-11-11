// Menu « Coller la tonalité » (clic droit) pour les menus Tonalité des plugins : lit le presse-papiers (copié depuis Orbe Détect : « Fm »)
// et appelle `apply`. Aucun paramètre n'est touché ailleurs.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include "KeyParse.h"

namespace kit {

class KeyPaster : public juce::MouseListener {
public:
    std::function<void(const keyparse::Key&)> apply;
    ~KeyPaster() override = default;
    std::shared_ptr<int> alive = std::make_shared<int>(0);
    void attach(juce::Component& c) { c.addMouseListener(this, false); }
    void mouseDown(const juce::MouseEvent& e) override {
        if (!e.mods.isPopupMenu() || !apply) return;
        keyparse::Key k; const bool ok = keyparse::parse(juce::SystemClipboard::getTextFromClipboard().toStdString(), k);
        juce::PopupMenu m;
        m.addItem(1, ok ? juce::String::fromUTF8("Coller la tonalit\xC3\xA9 : ") + juce::String(keyparse::format(k.pc, k.minor)) : juce::String::fromUTF8("Coller la tonalit\xC3\xA9 (copier d'abord depuis Orbe D\xC3\xA9tect)"), ok);
        // le menu est asynchrone : si l'éditeur se ferme avant le choix, le rappel ne touche plus rien (jeton partagé, comme SafePointer pour un objet qui n'est pas un composant)
        std::weak_ptr<int> w = alive;
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(e.eventComponent), [this, w, ok, k](int r) { if (r == 1 && ok && w.lock() && apply) apply(k); });
    }
};

}  // namespace kit
