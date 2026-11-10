# Orbe Accroc : accidents de lecture volontaires sur la voix (stutter, retour, arrêt bande, gel, bégaiement, crush, saut, trous), calés sur le tempo (VST3 + AU).
# Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine. Moteur sans JUCE : AccrocDsp.h.
set(_acrdir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeAccroc
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbeaccroc
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OAcr
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Accroc"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeAccroc PRIVATE ${_acrdir}/AccrocProcessor.cpp ${_acrdir}/AccrocEditor.cpp)
target_compile_definitions(OrbeAccroc PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeAccroc PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur (sans JUCE) : cmake --build . --target orbeaccroc_test
add_executable(orbeaccroc_test EXCLUDE_FROM_ALL ${_acrdir}/accroc_test.cpp)

# Contrôle visuel : cmake --build . --target orbeaccroc_snapshot (sous xvfb)
juce_add_console_app(orbeaccroc_snapshot PRODUCT_NAME "orbeaccroc_snapshot")
target_sources(orbeaccroc_snapshot PRIVATE ${_acrdir}/accroc_snapshot.cpp ${_acrdir}/AccrocProcessor.cpp ${_acrdir}/AccrocEditor.cpp)
set_target_properties(orbeaccroc_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbeaccroc_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbeaccroc_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
