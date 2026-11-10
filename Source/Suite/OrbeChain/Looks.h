// Looks de voix (noms originaux) : les 10 d'origine (index 0..9, jamais déplacés : compatibilité d'état), puis les looks Rap / Pop électro ajoutés à la suite.
// Chaque look transforme 4 macros (Intensité, Espace, Mordant, Couleur : 0..1) en réglages complets. Valeurs = hypothèses d'écoute (docs/looks-voix.md), à juger à l'oreille.
#pragma once
#include "OrbeChain.h"

namespace oc {

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline void qEq(S& s, float hp, float lsF, float lsG, float b1F, float b1G, float b1Q, float b2F, float b2G, float b2Q, float hsF, float hsG) {
    s.eqOn = 1; s.hp = hp; s.lsF = lsF; s.lsG = lsG; s.b1F = b1F; s.b1G = b1G; s.b1Q = b1Q; s.b2F = b2F; s.b2G = b2G; s.b2Q = b2Q; s.hsF = hsF; s.hsG = hsG;
}
inline void qComp(S& s, float thr, float ratio, float atk, float rel, float mk, float mix) { s.cpOn = 1; s.thr = thr; s.ratio = ratio; s.atk = atk; s.rel = rel; s.mk = mk; s.cpMix = mix; }
inline void qSat(S& s, float drive, float bias, float crush) { s.saOn = 1; s.drive = drive; s.bias = bias; s.crush = crush; s.saMix = 1; }
inline void qTune(S& s, float amt, float speed) { s.tnOn = 1; s.tnAmt = std::min(1.f, amt); s.tnSpeed = speed; }
inline void qDs(S& s, float f, float amt) { s.dsOn = 1; s.dsF = f; s.dsAmt = std::min(1.f, amt); }
inline void qDbl(S& s, float mix, float a, float b, float w) { s.dbOn = 1; s.dbMix = std::min(1.f, mix); s.dbA = a; s.dbB = b; s.dbWide = w; }
inline void qDly(S& s, float ms, int div, float fb, float mix, float duck) { s.dyOn = 1; s.dyMs = ms; s.dyDiv = (float)div; s.dyFb = fb; s.dyMix = mix; s.dyDuck = duck; }
inline void qRev(S& s, float size, float pre, float damp, float mix) { s.rvOn = 1; s.rvSize = size; s.rvPre = pre; s.rvDamp = damp; s.rvMix = mix; }

struct Look {
    const char* name; const char* tag; const char* mods;  // mods : modules actifs gravés (E C S T D 2 Y R)
    float def[4];       // macros par défaut : Intensité, Espace, Mordant, Couleur
    float trimDb;       // correction de sortie du preset Atelier (statique)
    float bias;         // décalage du niveau automatique d'Orbe Looks (dB, calibré par le test)
    void (*map)(float, float, float, float, S&);
    const char* cat = "Classiques";   // catégorie affichée dans la liste de l'éditeur
};

inline const std::vector<Look>& looks() {
    static const std::vector<Look> v = {
        {"Turbine", "Hauteur dure, doubles, échos d'ad-libs", "ECSTD2YR", {0.6f, 0.45f, 0.35f, 0.5f}, -5.8f, 0.5f,
         [](float i, float e, float m, float c, S& s) {
             qEq(s, 90, 150, 0, 300, -2, 1, 3500, 1 + 2 * c, 1, 9000, -2 + 7 * c); qComp(s, -20, 3 + 2 * i, 8, 90, 2 + 2 * i, 1);
             qSat(s, 0.05f + 0.3f * m, 0.1f, m > 0.7f ? (m - 0.7f) * 1.5f : 0.f); qTune(s, 0.5f + 0.5f * i, lerp(30, 2, i)); qDs(s, 6500, 0.4f);
             qDbl(s, 0.15f + 0.4f * i, 0.14f, -0.14f, 0.8f); qDly(s, 250, 4, 0.22f + 0.2f * e, 0.1f + 0.35f * e, 0.8f); qRev(s, 0.8f + 1.4f * e, 15, 0.45f, 0.08f + 0.3f * e); }},
        {"Béton", "Voix sèche, proche, compressée", "ECSDYR", {0.5f, 0.3f, 0.3f, 0.5f}, -4.7f, 1.7f,
         [](float i, float e, float m, float c, S& s) {
             qEq(s, 90, 150, 0, 300, -1.5f, 1, 3200, 1.5f + 2 * (c - 0.5f), 1, 10000, -1 + 4 * c); qComp(s, -24, 3.5f + 2 * i, 15, 100, 3 + 2 * i, 1);
             qSat(s, 0.05f + 0.25f * m, 0, 0); qDs(s, 7000, 0.4f); qDbl(s, 0.12f * i, 0.1f, -0.1f, 0.3f);
             qDly(s, 90, 0, 0.05f, 0.05f + 0.15f * e, 0.5f); qRev(s, 0.5f, 20, 0.6f, 0.03f + 0.1f * e); }},
        {"Nacre", "Chant aérien, large, brillant", "ECSTD2YR", {0.5f, 0.55f, 0.3f, 0.6f}, -4.5f, 0.5f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 0.3f + 0.3f * i, lerp(80, 25, i)); qEq(s, 110, 150, 0, 250, -2, 1, 3000, 1, 1, 11000, 7 * c);
             qComp(s, -22, 2.5f, 25, 160, 2, 0.8f + 0.2f * i); qSat(s, 0.08f + 0.2f * m, 0.1f, 0); qDs(s, 7500, 0.5f);
             qDbl(s, 0.3f + 0.4f * i, 0.2f, -0.2f, 1); qDly(s, 250, 3, 0.3f + 0.2f * e, 0.1f + 0.3f * e, 0.7f); qRev(s, 1.4f + 2.4f * e, 30, 0.35f, 0.15f + 0.35f * e); }},
        {"Verre Pilé", "Hauteur nette, couches hautes, grain", "ECSTD2YR", {0.6f, 0.35f, 0.5f, 0.6f}, -6.5f, 2.9f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 1, lerp(4, 1, i)); qEq(s, 120, 200, -2, 800, -1, 1, 5000, 2, 1, 10000, 3 + 6 * c); qComp(s, -26, 6, 3, 60, 4, 1);
             qSat(s, 0.3f + 0.5f * m, 0.2f, 0.1f + 0.5f * m); qDs(s, 8000, 0.5f); qDbl(s, 0.2f + 0.4f * i, 12, 7, 0.9f);
             qDly(s, 200, 1, 0.25f, 0.08f + 0.2f * e, 0.5f); qRev(s, 0.7f + 1.3f * e, 10, 0.4f, 0.06f + 0.2f * e); }},
        {"Cave", "Sombre, serrée, grave en avant", "ECSDYR2", {0.5f, 0.35f, 0.4f, 0.3f}, -7.1f, 0.3f,
         [](float i, float e, float m, float c, S& s) {
             qEq(s, 80, 150, 2, 400, -2, 1, 3000, 2, 1, 9000, -6 + 8 * c); qComp(s, -22, 4 + 2 * i, 10, 80, 3, 1);
             qSat(s, 0.15f + 0.3f * m, 0.15f, 0); qDs(s, 7000, 0.4f); qDbl(s, 0.05f + 0.15f * i, 0.1f, -0.1f, 0.2f);
             qDly(s, 110, 0, 0.1f, 0.05f + 0.2f * e, 0.6f); qRev(s, 0.6f + 1.4f * e, 25, 0.7f, 0.06f + 0.28f * e); }},
        {"Combiné", "Bande étroite, grain de combiné", "ECSYR", {0.5f, 0.3f, 0.5f, 0.5f}, -5.0f, 0.6f,
         [](float i, float e, float m, float c, S& s) {
             qEq(s, 150 + 250 * i, 300, -3, 1000, 3 + 3 * c, 0.8f, 2500, 3, 1, 3400, -6 - 14 * i); qComp(s, -26, 5, 5, 70, 4, 1);
             qSat(s, 0.3f + 0.4f * m, 0.2f, 0.2f + 0.5f * m); qDly(s, 250, 2, 0.2f, 0.15f * e, 0.5f); qRev(s, 0.5f + 1.f * e, 5, 0.8f, 0.05f + 0.2f * e); }},
        {"Alcôve", "Murmure proche, très présent", "ECSD2YR", {0.5f, 0.3f, 0.3f, 0.55f}, -3.7f, 2.7f,
         [](float i, float e, float m, float c, S& s) {
             qEq(s, 100, 150, 0, 220, -2, 1, 4000, 1.5f + 3 * (c - 0.5f), 1, 12000, 1 + 4 * c); qComp(s, -28, 4, 12, 140, 5, 1);
             qSat(s, 0.1f + 0.2f * m, 0, 0); qDs(s, 7000, 0.6f + 0.3f * i); qDbl(s, 0.15f + 0.3f * i, 0.08f, -0.08f, 0.5f);
             qDly(s, 70, 0, 0, 0.06f + 0.14f * e, 0.5f); qRev(s, 0.4f + 0.9f * e, 20, 0.5f, 0.04f + 0.16f * e); }},
        {"Braise", "Saturée, mélodique, trouble", "ECSTD2YR", {0.6f, 0.4f, 0.55f, 0.5f}, -9.6f, 1.6f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 0.6f + 0.4f * i, lerp(25, 5, i)); qEq(s, 110, 150, 0, 500, -2, 1, 2500, 3, 1.2f, 8000, -1 + 4 * c); qComp(s, -24, 4, 20, 70, 3, 1);
             qSat(s, 0.45f + 0.5f * m, 0.3f, 0.1f + 0.35f * m); qDs(s, 6500, 0.5f); qDbl(s, 0.3f + 0.4f * i, 0.3f, -0.3f, 1);
             qDly(s, 250, 2, 0.3f + 0.2f * e, 0.08f + 0.25f * e, 0.6f); qRev(s, 1.2f + 2.f * e, 20, 0.5f, 0.08f + 0.28f * e); }},
        {"Sépia", "Chaleur de bande, pièce courte", "ECSDYR", {0.5f, 0.35f, 0.4f, 0.45f}, -8.2f, 0.2f,
         [](float i, float e, float m, float c, S& s) {
             (void)i; qEq(s, 70, 150, 2.5f, 350, -1, 1, 2500, 1.5f, 1, 9000, -5 + 7 * c); qComp(s, -20, 3, 20, 120, 2, 1);
             qSat(s, 0.2f + 0.4f * m, 0.1f, 0); qDs(s, 6500, 0.35f); qDly(s, 100, 0, 0.15f, 0.04f + 0.12f * e, 0.5f); qRev(s, 0.5f + 1.f * e, 12, 0.5f, 0.06f + 0.2f * e); }},
        {"Satin", "Intime, doublée, très sèche", "ECSTD2YR", {0.5f, 0.3f, 0.3f, 0.5f}, -3.8f, 0.4f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 0.25f + 0.35f * i, lerp(60, 20, i)); qEq(s, 100, 150, 0, 400, -2, 1, 3500, 1, 1, 10000, 1 + 4 * c); qComp(s, -20, 2.2f, 20, 130, 1, 1);
             qSat(s, 0.08f + 0.2f * m, 0.05f, 0); qDs(s, 6500, 0.5f); qDbl(s, 0.35f + 0.35f * i, 0.07f, -0.07f, 0.9f);
             qDly(s, 250, 3, 0.2f, 0.12f * e, 0.7f); qRev(s, 0.6f + 1.2f * e, 25, 0.5f, 0.02f + 0.2f * e); }},
        // ---- Rap ----
        {"Obsidienne", "Trap sombre, graves pleins, écho de noire", "ECSTD2YR", {0.55f, 0.45f, 0.35f, 0.35f}, -6.9f, 1.4f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 0.4f + 0.3f * i, lerp(30, 12, i)); qEq(s, 85, 120, 2.5f, 400, -2.5f, 1, 3000, 1.5f, 1, 8000, -3 + 6 * c);
             qComp(s, -22, 4 + 2 * i, 10, 80, 3, 1); qSat(s, 0.15f + 0.3f * m, 0.15f, 0); qDs(s, 6500, 0.4f); qDbl(s, 0.1f + 0.25f * i, 0.1f, -0.1f, 0.5f);
             qDly(s, 250, 4, 0.35f + 0.2f * e, 0.06f + 0.22f * e, 0.8f); qRev(s, 1.4f + 2.f * e, 20, 0.75f, 0.06f + 0.22f * e); }, "Rap"},
        {"Poussière", "Boom-bap voilé, médiums gras, poussière", "ECSDYR", {0.5f, 0.3f, 0.45f, 0.4f}, -8.5f, 0.5f,
         [](float i, float e, float m, float c, S& s) {
             (void)i; qEq(s, 80, 150, 2, 800, 1.5f, 0.8f, 3500, -1, 1, 7000, -4 + 5 * c); qComp(s, -20, 3, 25, 130, 2, 1);
             qSat(s, 0.25f + 0.35f * m, 0.2f, 0.05f + 0.25f * m); qDs(s, 6500, 0.3f); qDly(s, 95, 0, 0.1f, 0.04f + 0.12f * e, 0.5f); qRev(s, 0.5f + 1.f * e, 10, 0.7f, 0.05f + 0.16f * e); }, "Rap"},
        {"Givre", "Voix froide et fine, attaque sèche", "ECSDYR", {0.55f, 0.3f, 0.35f, 0.5f}, -3.8f, 0.6f,
         [](float i, float e, float m, float c, S& s) {
             qEq(s, 130, 200, -2, 350, -2, 1, 4000, 2.5f, 1.2f, 10000, 1 + 4 * c); qComp(s, -26, 5 + 2 * i, 6, 70, 4, 1);
             qSat(s, 0.12f + 0.25f * m, 0, 0); qDs(s, 7500, 0.5f); qDly(s, 250, 2, 0.15f, 0.03f + 0.15f * e, 0.8f); qRev(s, 0.7f + 1.2f * e, 35, 0.35f, 0.04f + 0.2f * e); }, "Rap"},
        {"Brume", "Voix flottante dans la brume, large et floue", "ECSTD2YR", {0.5f, 0.6f, 0.3f, 0.35f}, -4.6f, 1.7f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 0.35f + 0.3f * i, lerp(60, 20, i)); qEq(s, 100, 150, 0, 250, -1.5f, 1, 3000, -1.5f, 1, 9000, -4 + 6 * c);
             qComp(s, -22, 2.5f, 25, 150, 2, 0.8f); qSat(s, 0.1f + 0.2f * m, 0.1f, 0); qDs(s, 7000, 0.4f); qDbl(s, 0.3f + 0.35f * i, 0.22f, -0.22f, 1);
             qDly(s, 250, 3, 0.35f + 0.2f * e, 0.1f + 0.25f * e, 0.7f); qRev(s, 2.f + 3.f * e, 40, 0.6f, 0.1f + 0.25f * e); }, "Rap"},
        {"Mirage", "Autotune mélodique lisse et brillant", "ECSTD2YR", {0.6f, 0.45f, 0.35f, 0.55f}, -6.1f, 1.4f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 0.7f + 0.3f * i, lerp(18, 4, i)); qEq(s, 100, 150, 0, 400, -2, 1, 3000, 1 + 2 * (c - 0.5f), 1, 9500, 1 + 5 * c);
             qComp(s, -22, 3.5f, 12, 90, 3, 1); qSat(s, 0.1f + 0.25f * m, 0.1f, 0); qDs(s, 6500, 0.45f); qDbl(s, 0.2f + 0.35f * i, 0.12f, -0.12f, 0.9f);
             qDly(s, 250, 2, 0.3f + 0.2f * e, 0.08f + 0.25f * e, 0.7f); qRev(s, 1.2f + 2.f * e, 20, 0.45f, 0.06f + 0.22f * e); }, "Rap"},
        {"Cassette", "Grain de bande usée, bas débit", "ECSYR", {0.5f, 0.3f, 0.5f, 0.4f}, -6.4f, 0.4f,
         [](float i, float e, float m, float c, S& s) {
             (void)i; qEq(s, 180, 250, -1, 1200, 2.5f, 0.9f, 3000, 1.5f, 1, 5500, -10 + 6 * c); qComp(s, -24, 4, 8, 100, 3, 1);
             qSat(s, 0.3f + 0.4f * m, 0.25f, 0.25f + 0.45f * m); qDly(s, 250, 2, 0.2f, 0.05f + 0.15f * e, 0.6f); qRev(s, 0.5f + 0.9f * e, 8, 0.8f, 0.04f + 0.18f * e); }, "Rap"},
        {"Ruelle", "Ad-libs : slap et échos de noire en avant", "ECSDYR", {0.5f, 0.55f, 0.35f, 0.5f}, -6.2f, 0.6f,
         [](float i, float e, float m, float c, S& s) {
             (void)i; qEq(s, 140, 200, -1, 500, -1, 1, 2800, 2.5f, 1, 9000, 1 + 3 * c); qComp(s, -22, 4, 6, 80, 3, 1);
             qSat(s, 0.15f + 0.3f * m, 0.1f, 0); qDs(s, 6500, 0.4f); qDly(s, 250, 4, 0.4f + 0.25f * e, 0.2f + 0.4f * e, 0.5f); qRev(s, 1.f + 1.8f * e, 25, 0.5f, 0.05f + 0.2f * e); }, "Rap"},
        {"Gueulante", "Rap frontal, compression serrée, mordant", "ECSDYR", {0.6f, 0.2f, 0.5f, 0.5f}, -8.3f, 0.4f,
         [](float i, float e, float m, float c, S& s) {
             qEq(s, 100, 120, 1.5f, 350, -2, 1, 3000, 3, 1, 9000, 1 + 3 * c); qComp(s, -28, 6 + 3 * i, 4, 50, 6, 1);
             qSat(s, 0.35f + 0.5f * m, 0.2f, 0); qDs(s, 7000, 0.5f); qDly(s, 80, 0, 0.05f, 0.04f + 0.1f * e, 0.5f); qRev(s, 0.4f + 0.6f * e, 10, 0.6f, 0.02f + 0.1f * e); }, "Rap"},

        // ---- Pop électro ----
        {"Paillettes", "Hyperpop étincelant, octave haute, ultra net", "ECSTD2YR", {0.6f, 0.4f, 0.4f, 0.7f}, -6.0f, 1.3f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 1, lerp(5, 1, i)); qEq(s, 130, 200, -2, 600, -1.5f, 1, 5500, 2.5f, 1, 11000, 4 + 6 * c); qComp(s, -26, 5, 4, 60, 4, 1);
             qSat(s, 0.2f + 0.4f * m, 0.1f, 0.05f + 0.3f * m); qDs(s, 8000, 0.5f); qDbl(s, 0.3f + 0.35f * i, 12, 0.2f, 1);
             qDly(s, 200, 1, 0.3f, 0.05f + 0.16f * e, 0.5f); qRev(s, 0.8f + 1.4f * e, 10, 0.3f, 0.05f + 0.2f * e); }, "Pop électro"},
        {"Rêverie", "Pop onirique, halo de réverbération long", "ECSTD2YR", {0.5f, 0.6f, 0.25f, 0.6f}, -3.6f, 0.5f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 0.2f + 0.3f * i, lerp(90, 40, i)); qEq(s, 110, 180, 1, 300, -2, 1, 3500, 0.5f, 1, 10000, 2 + 5 * c);
             qComp(s, -22, 2.2f, 25, 180, 2, 0.8f + 0.2f * i); qSat(s, 0.06f + 0.15f * m, 0.1f, 0); qDs(s, 7500, 0.45f); qDbl(s, 0.35f + 0.4f * i, 0.2f, -0.2f, 1);
             qDly(s, 250, 3, 0.4f + 0.2f * e, 0.1f + 0.22f * e, 0.6f); qRev(s, 2.2f + 3.5f * e, 35, 0.35f, 0.12f + 0.28f * e); }, "Pop électro"},
        {"Opale", "Synth-pop nacré, voix lustrée et aérienne", "ECSTD2YR", {0.55f, 0.4f, 0.3f, 0.6f}, -4.5f, 1.7f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 0.5f + 0.3f * i, lerp(35, 12, i)); qEq(s, 120, 160, 0, 280, -1.5f, 1, 4000, 1.5f, 1, 11000, 3 + 5 * c);
             qComp(s, -22, 3, 12, 100, 2.5f, 1); qSat(s, 0.08f + 0.18f * m, 0.1f, 0); qDs(s, 7500, 0.5f); qDbl(s, 0.25f + 0.3f * i, 0.1f, -0.1f, 0.9f);
             qDly(s, 250, 2, 0.25f + 0.2f * e, 0.06f + 0.2f * e, 0.7f); qRev(s, 0.9f + 1.5f * e, 20, 0.4f, 0.06f + 0.22f * e); }, "Pop électro"},
        {"Ondes", "Voix de radio, bande médium compressée", "ECSYR", {0.5f, 0.2f, 0.4f, 0.5f}, -5.5f, 0.5f,
         [](float i, float e, float m, float c, S& s) {
             (void)i; qEq(s, 250, 300, -3, 1500, 3, 1, 2800, 3, 1, 5000, -10 + 5 * c); qComp(s, -26, 5, 5, 70, 5, 1);
             qSat(s, 0.25f + 0.3f * m, 0.15f, 0.05f + 0.2f * m); qRev(s, 0.4f + 0.6f * e, 5, 0.8f, 0.03f + 0.12f * e); }, "Pop électro"},
        {"Allô", "Voix de téléphone, bande très étroite", "ECSYR", {0.6f, 0.15f, 0.4f, 0.5f}, -2.6f, 0.6f,
         [](float i, float e, float m, float c, S& s) {
             qEq(s, 400 + 150 * i, 300, -4, 1100, 5, 0.8f, 2400, 3, 1, 2800, -12 - 8 * (1 - c)); qComp(s, -28, 6, 3, 60, 5, 1);
             qSat(s, 0.3f + 0.3f * m, 0.1f, 0.15f + 0.35f * m); qRev(s, 0.3f + 0.5f * e, 3, 0.9f, 0.03f + 0.1f * e); }, "Pop électro"},
        {"Soupir", "Souffle proche, très présent, très sec", "ECSD2R", {0.5f, 0.25f, 0.3f, 0.6f}, 0.0f, 1.1f,
         [](float i, float e, float m, float c, S& s) {
             qEq(s, 110, 150, -1, 250, -2, 1, 4500, 2, 1, 12000, 2 + 4 * c); qComp(s, -30, 5, 10, 150, 6, 1);
             qSat(s, 0.05f + 0.12f * m, 0, 0); qDs(s, 6500, 0.7f + 0.25f * i); qDbl(s, 0.2f + 0.3f * i, 0.06f, -0.06f, 0.6f); qRev(s, 0.4f + 0.8f * e, 15, 0.5f, 0.03f + 0.14f * e); }, "Pop électro"},
        {"Chorale", "Chœur large, voix multipliées en éventail", "ECSTD2YR", {0.6f, 0.55f, 0.25f, 0.5f}, -3.5f, 0.5f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 0.3f + 0.3f * i, lerp(70, 30, i)); qEq(s, 100, 200, 0, 300, -1.5f, 1, 3000, 1, 1, 10000, 1 + 4 * c);
             qComp(s, -22, 2.5f, 20, 150, 2, 1); qSat(s, 0.05f + 0.1f * m, 0.05f, 0); qDs(s, 7000, 0.4f); qDbl(s, 0.6f + 0.4f * i, 0.3f, -0.3f, 1);
             qDly(s, 250, 4, 0.25f, 0.04f + 0.12f * e, 0.7f); qRev(s, 1.8f + 3.f * e, 30, 0.4f, 0.1f + 0.28f * e); }, "Pop électro"},
        {"Automate", "Voix robotique douce, hauteur verrouillée", "ECSTD2YR", {0.65f, 0.3f, 0.3f, 0.5f}, -6.9f, 1.6f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 1, lerp(3, 1, i)); qEq(s, 120, 150, 0, 400, -2, 1, 3000, 2, 1, 10000, 2 + 4 * c); qComp(s, -24, 4, 8, 80, 3, 1);
             qSat(s, 0.1f + 0.25f * m, 0, 0.05f + 0.2f * m); qDs(s, 7000, 0.4f); qDbl(s, 0.15f + 0.25f * i, 0.4f, -0.4f, 0.9f);
             qDly(s, 250, 2, 0.2f, 0.05f + 0.15f * e, 0.7f); qRev(s, 0.6f + 1.f * e, 10, 0.5f, 0.05f + 0.2f * e); }, "Pop électro"},
        {"Nocturne", "Pop alternatif nocturne, velouté et sombre", "ECSTD2YR", {0.5f, 0.45f, 0.3f, 0.35f}, -5.9f, 0.5f,
         [](float i, float e, float m, float c, S& s) {
             qTune(s, 0.25f + 0.3f * i, lerp(60, 25, i)); qEq(s, 100, 150, 1.5f, 450, -2, 1, 3000, 1, 1, 8500, -4 + 5 * c);
             qComp(s, -22, 3, 18, 130, 2.5f, 1); qSat(s, 0.12f + 0.25f * m, 0.15f, 0); qDs(s, 6500, 0.45f); qDbl(s, 0.25f + 0.3f * i, 0.09f, -0.09f, 0.8f);
             qDly(s, 250, 4, 0.3f + 0.2f * e, 0.06f + 0.2f * e, 0.7f); qRev(s, 1.6f + 2.5f * e, 25, 0.65f, 0.07f + 0.22f * e); }, "Pop électro"},
    };
    return v;
}
inline int numLooks() { return (int)looks().size(); }

// Réglages complets d'un look pour des macros données.
inline S lookSettings(int idx, float i, float e, float m, float c) {
    S s; const auto& l = looks()[(size_t)std::clamp(idx, 0, numLooks() - 1)]; l.map(i, e, m, c, s); s.lvBias = l.bias; s.agInit = l.trimDb + l.bias; return s;
}
inline S lookDefaults(int idx) {
    const auto& l = looks()[(size_t)std::clamp(idx, 0, numLooks() - 1)];
    S s = lookSettings(idx, l.def[0], l.def[1], l.def[2], l.def[3]); s.out = l.trimDb; return s;
}

}  // namespace oc
