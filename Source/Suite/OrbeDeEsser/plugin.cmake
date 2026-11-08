# Orbe DeEsser : atténuateur de sibilance voix (VST3 + AU). Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine.
# Briques partagées (DSP + face avant) : Source/Suite/OrbeDsp/*.h, header-only, rien à déclarer.
set(_desdir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeDeEsser
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbedeesser
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE ODes
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe DeEsser"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeDeEsser PRIVATE ${_desdir}/DeEsserProcessor.cpp ${_desdir}/DeEsserEditor.cpp)
target_compile_definitions(OrbeDeEsser PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeDeEsser PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur (sans JUCE) : cmake --build . --target orbedeesser_test
add_executable(orbedeesser_test EXCLUDE_FROM_ALL ${_desdir}/deesser_test.cpp)

# Contrôle visuel : cmake --build . --target orbedeesser_snapshot (sous xvfb)
juce_add_console_app(orbedeesser_snapshot PRODUCT_NAME "orbedeesser_snapshot")
target_sources(orbedeesser_snapshot PRIVATE ${_desdir}/deesser_snapshot.cpp ${_desdir}/DeEsserProcessor.cpp ${_desdir}/DeEsserEditor.cpp)
set_target_properties(orbedeesser_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbedeesser_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbedeesser_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
