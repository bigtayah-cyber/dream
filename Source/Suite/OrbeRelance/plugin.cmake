# Orbe Relance : ad-libs et réponses générés à partir des fins de phrase de la voix principale (VST3 + AU).
# Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine. Moteur sans JUCE : RelanceDsp.h.
set(_reldir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeRelance
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orberelance
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OREL
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Relance"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeRelance PRIVATE ${_reldir}/RelanceProcessor.cpp ${_reldir}/RelanceEditor.cpp)
target_compile_definitions(OrbeRelance PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeRelance PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur (sans JUCE) : cmake --build . --target orbe_relance_test
add_executable(orbe_relance_test EXCLUDE_FROM_ALL ${_reldir}/relance_test.cpp)

# Contrôle visuel : cmake --build . --target orberelance_snapshot (sous xvfb)
juce_add_console_app(orberelance_snapshot PRODUCT_NAME "orberelance_snapshot")
target_sources(orberelance_snapshot PRIVATE ${_reldir}/relance_snapshot.cpp ${_reldir}/RelanceProcessor.cpp ${_reldir}/RelanceEditor.cpp)
set_target_properties(orberelance_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orberelance_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orberelance_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
