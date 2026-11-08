# Orbe EQ : égaliseur paramétrique interactif 8 bandes (VST3 + AU). Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine.
# Briques partagées (DSP + face avant) : Source/Suite/OrbeDsp/*.h, header-only, rien à déclarer.
set(_eqdir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeEQ
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbeeq
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OEQ_
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe EQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeEQ PRIVATE ${_eqdir}/EqProcessor.cpp ${_eqdir}/EqEditor.cpp)
target_compile_definitions(OrbeEQ PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeEQ PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur (sans JUCE) : cmake --build . --target orbeeq_test
add_executable(orbeeq_test EXCLUDE_FROM_ALL ${_eqdir}/eq_test.cpp)

# Contrôle visuel : cmake --build . --target orbeeq_snapshot (sous xvfb)
juce_add_console_app(orbeeq_snapshot PRODUCT_NAME "orbeeq_snapshot")
target_sources(orbeeq_snapshot PRIVATE ${_eqdir}/eq_snapshot.cpp ${_eqdir}/EqProcessor.cpp ${_eqdir}/EqEditor.cpp)
set_target_properties(orbeeq_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbeeq_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbeeq_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
