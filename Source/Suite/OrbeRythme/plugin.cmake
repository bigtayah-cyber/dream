# Orbe Rythme : générateur de batterie et de rythmes, en MIDI (VST3 + AU). Même moteur qu'Orbe (Source/Orbe/OrbeCore.h, OrbeSynth.h).
# Inclus par CMakeLists.txt (glob Source/Suite/*/plugin.cmake).
set(ORYT_DIR ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeRythme
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orberythme
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE ORyt
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Rythme"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    VST3_CATEGORIES Instrument Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeRythme PRIVATE ${ORYT_DIR}/OrbeRythmeProcessor.cpp ${ORYT_DIR}/OrbeRythmeEditor.cpp)
target_compile_definitions(OrbeRythme PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeRythme PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Tests console (état, migration depuis un ancien Orbe, audio, MIDI) : cmake --build . --target orberythme_test && ./orberythme_test_artefacts/orberythme_test
# Lie aussi le processeur d'Orbe (pour fabriquer de vrais états d'ancien Orbe) : une seule fabrique de plugin, celle d'Orbe.
juce_add_console_app(orberythme_test PRODUCT_NAME "orberythme_test")
target_sources(orberythme_test PRIVATE ${ORYT_DIR}/test_orberythme.cpp ${ORYT_DIR}/OrbeRythmeProcessor.cpp ${ORYT_DIR}/OrbeRythmeEditor.cpp
    ${ORYT_DIR}/../../Orbe/OrbeProcessor.cpp ${ORYT_DIR}/../../Orbe/OrbeEditor.cpp)
set_target_properties(orberythme_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orberythme_test PRIVATE ORBE_NO_FACTORY=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orberythme_test PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

# Contrôle visuel : cmake --build . --target orberythme_snapshot, puis DSG_THEME=clair|sombre|classique xvfb-run -a ./orberythme_snapshot sortie.png [largeur]
juce_add_console_app(orberythme_snapshot PRODUCT_NAME "orberythme_snapshot")
target_sources(orberythme_snapshot PRIVATE ${ORYT_DIR}/snapshot_orberythme.cpp ${ORYT_DIR}/OrbeRythmeProcessor.cpp ${ORYT_DIR}/OrbeRythmeEditor.cpp)
set_target_properties(orberythme_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orberythme_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orberythme_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
