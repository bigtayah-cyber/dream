# Orbe Looks : chaîne vocale à looks, 4 macros (VST3 + AU). Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine.
set(_lksdir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeLooks
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbelooks
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OLks
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Looks"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeLooks PRIVATE ${_lksdir}/LooksProcessor.cpp ${_lksdir}/LooksEditor.cpp)
target_compile_definitions(OrbeLooks PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeLooks PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur commun Orbe Chain (sans JUCE), déclaré ici une seule fois : cmake --build . --target orbechain_test
add_executable(orbechain_test EXCLUDE_FROM_ALL ${_lksdir}/../OrbeChain/chain_test.cpp)

# Contrôle visuel : cmake --build . --target orbelooks_snapshot (sous xvfb)
juce_add_console_app(orbelooks_snapshot PRODUCT_NAME "orbelooks_snapshot")
target_sources(orbelooks_snapshot PRIVATE ${_lksdir}/looks_snapshot.cpp ${_lksdir}/LooksProcessor.cpp ${_lksdir}/LooksEditor.cpp)
set_target_properties(orbelooks_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbelooks_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbelooks_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
