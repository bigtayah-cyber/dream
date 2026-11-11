# Orbe FX : effet audio (filtre / saturation / chorus / delay / réverbe, 4 macros, préréglages), VST3 + AU. DSP original : OrbeFxDsp.h.
# Remplace l'ancien « Orbe FX » (le moteur d'Orbe déclaré comme effet, qui ignorait l'audio) : mêmes PLUGIN_CODE et bundle.
# Inclus par CMakeLists.txt (glob Source/Suite/*/plugin.cmake).
set(ORFX_DIR ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeFX
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbefx
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OrbF
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeFX PRIVATE ${ORFX_DIR}/OrbeFxProcessor.cpp ${ORFX_DIR}/OrbeFxEditor.cpp)
target_compile_definitions(OrbeFX PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeFX PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Tests console (DSP, état, audio) + contrôle visuel : cmake --build . --target orbefx_test && ./orbefx_test_artefacts/orbefx_test
#   orbefx_test --snap sortie.png [largeur] [préréglage]   (DSG_THEME=clair|sombre|classique, xvfb-run -a)
juce_add_console_app(orbefx_test PRODUCT_NAME "orbefx_test")
target_sources(orbefx_test PRIVATE ${ORFX_DIR}/test_orbefx.cpp ${ORFX_DIR}/OrbeFxProcessor.cpp ${ORFX_DIR}/OrbeFxEditor.cpp)
set_target_properties(orbefx_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbefx_test PRIVATE ORBE_NO_FACTORY=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbefx_test PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
