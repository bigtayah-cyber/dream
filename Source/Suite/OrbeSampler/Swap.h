// Publication sans verrou d'un objet immuable (banque de Pads, rendu de Sample) vers le thread audio. C++17 pur, sans JUCE.
//
// Le thread audio lit un pointeur brut ; les autres threads gardent les propriétaires (shared_ptr). Durée de vie PAR GÉNÉRATIONS :
//  - chaque objet publié reçoit un numéro de génération croissant (champ `mutable std::uint64_t gen` de T) ;
//  - à la fin de chaque bloc, le thread audio annonce la plus ancienne génération encore lue par une voix (reportInUse) ;
//  - publish() ne libère que les objets plus anciens que cela (et jamais l'objet courant).
// Une voix peut donc durer 240 s ou plus : sa banque reste vivante tant qu'elle joue, sans délai arbitraire ni verrou côté audio.
// Ordre des écritures : le pointeur brut est publié AVANT le numéro courant ; ainsi un numéro lu par le thread audio garantit
// que le pointeur lu ensuite est au moins aussi récent (et donc non libéré).
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace osamp {

template <class T>
class Swap {
public:
    // Thread message ou de rendu.
    void publish(std::shared_ptr<const T> p) {
        std::lock_guard<std::mutex> l(m);
        const std::uint64_t g = ++counter;
        p->gen = g;
        raw.store(p.get(), std::memory_order_release);
        curGen.store(g, std::memory_order_release);
        cur = p;
        hist.push_back(std::move(p));
        prune();
    }
    std::shared_ptr<const T> share() const { std::lock_guard<std::mutex> l(m); return cur; }
    // Nombre d'objets gardés vivants (courant compris) : pour les tests.
    int liveCount() const { std::lock_guard<std::mutex> l(m); return (int)hist.size(); }

    // Thread audio : lecture sans verrou.
    const T* get() const { return raw.load(std::memory_order_acquire); }
    std::uint64_t currentGen() const { return curGen.load(std::memory_order_acquire); }
    // Fin de bloc : plus ancienne génération encore lue par une voix (ou currentGen() si aucune).
    void reportInUse(std::uint64_t oldest) { inUse.store(oldest, std::memory_order_release); }
    // prepareToPlay : aucune voix ne tient plus d'objet (le thread audio a remis ses pointeurs à zéro AVANT cet appel).
    void quiesce() { inUse.store(currentGen(), std::memory_order_release); std::lock_guard<std::mutex> l(m); prune(); }

private:
    void prune() {  // m doit être pris
        const std::uint64_t keep = inUse.load(std::memory_order_acquire);
        hist.erase(std::remove_if(hist.begin(), hist.end(), [&](const std::shared_ptr<const T>& i) { return i != cur && i->gen < keep; }), hist.end());
    }
    mutable std::mutex m;
    std::atomic<const T*> raw{nullptr};
    std::atomic<std::uint64_t> curGen{0}, inUse{0};   // inUse = 0 tant que le thread audio n'a rien annoncé : on ne libère rien
    std::uint64_t counter = 0;
    std::shared_ptr<const T> cur;
    std::vector<std::shared_ptr<const T>> hist;
};

}  // namespace osamp
