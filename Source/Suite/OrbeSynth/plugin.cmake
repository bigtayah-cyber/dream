# Orbe Synth : synthé polyphonique (suite Orbe). Inclus automatiquement par le CMakeLists racine.
set(OSYN_DIR ${CMAKE_CURRENT_LIST_DIR})

juce_add_plugin(OrbeSynth
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbesynth
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OSyn
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeSynth PRIVATE ${OSYN_DIR}/OrbeSynthProcessor.cpp ${OSYN_DIR}/OrbeSynthEditor.cpp)
target_compile_definitions(OrbeSynth PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeSynth PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur (sans JUCE) : cmake --build . --target orbesynth_test
add_executable(orbesynth_test EXCLUDE_FROM_ALL ${OSYN_DIR}/orbesynth_test.cpp)

# Contrôle visuel : cmake --build . --target orbesynth_snapshot (exécuter sous xvfb)
juce_add_console_app(orbesynth_snapshot PRODUCT_NAME "orbesynth_snapshot")
target_sources(orbesynth_snapshot PRIVATE ${OSYN_DIR}/snapshot.cpp ${OSYN_DIR}/OrbeSynthProcessor.cpp ${OSYN_DIR}/OrbeSynthEditor.cpp)
set_target_properties(orbesynth_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbesynth_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbesynth_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
