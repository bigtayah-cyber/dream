// Test du patron de version d'état (Kit/StateVersion.h) : états anciens relus, version écrite, identifiants intacts.
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "../Source/Kit/StateVersion.h"
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("ECHEC: " __VA_ARGS__); std::printf("\n"); } else { std::printf("ok     " __VA_ARGS__); std::printf("\n"); } } while (0)
int main() {
    // état « d'avant » : arbre APVTS sans stateVersion, avec des paramètres et une propriété annexe
    const char* oldXml = "<ORBEMASTER><PARAM id=\"eq0\" value=\"2.5\"/><PARAM id=\"ceil\" value=\"-1\"/><PARAM id=\"limOn\" value=\"1\"/></ORBEMASTER>";
    auto xml = juce::XmlDocument::parse(oldXml);
    auto tree = juce::ValueTree::fromXml(*xml);
    CHECK(kit::stateVersionOf(tree) == 0, "etat sans version = 0 (projet d'avant)");
    auto copy = tree.createCopy();
    kit::stampState(copy);
    CHECK(kit::stateVersionOf(copy) == kit::kStateVersion && kit::kStateVersion == 2, "version ecrite = %d", kit::stateVersionOf(copy));
    bool same = copy.getNumChildren() == tree.getNumChildren();
    for (int i = 0; same && i < tree.getNumChildren(); ++i) same = tree.getChild(i).isEquivalentTo(copy.getChild(i));
    CHECK(same, "les identifiants et valeurs des parametres sont intacts apres le tampon");
    // aller-retour binaire/XML comme getStateInformation / setStateInformation
    auto out = copy.createXml()->toString(juce::XmlElement::TextFormat().singleLine());
    auto back = juce::ValueTree::fromXml(*juce::XmlDocument::parse(out));
    CHECK(kit::stateVersionOf(back) == 2 && back.getChild(1).getProperty("id").toString() == "ceil", "aller-retour XML : version 2 relue, parametres presents");
    // version explicite (projet ouvert dont le moteur ancien est gardé)
    auto kept = tree.createCopy(); kit::stampState(kept, 1);
    CHECK(kit::stateVersionOf(kept) == 1, "version explicite conservee");
    // état écrit par un plugin plus récent : lisible, version plus grande que celle connue
    auto fut = tree.createCopy(); kit::stampState(fut, 7);
    CHECK(kit::stateVersionOf(fut) == 7, "etat plus recent relu sans erreur");
    std::printf(fails ? "%d ECHEC(S)\n" : "TOUT EST BON\n", fails); return fails ? 1 : 0;
}
