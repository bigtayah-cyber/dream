# Orbe 808 : basse 808 monophonique (VST3 + AU). Inclus par CMakeLists.txt (glob Source/Suite/*/plugin.cmake).
set(O808_DIR ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(Orbe808
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbe808
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE O808
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe 808"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(Orbe808 PRIVATE ${O808_DIR}/Orbe808Processor.cpp ${O808_DIR}/Orbe808Editor.cpp)
target_compile_definitions(Orbe808 PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Orbe808 PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test du moteur sans JUCE : cmake --build . --target orbe808_test && ./orbe808_test
add_executable(orbe808_test EXCLUDE_FROM_ALL ${O808_DIR}/test_orbe808.cpp)

# Contrôle visuel : cmake --build . --target orbe808_snapshot, puis xvfb-run -a ./orbe808_snapshot sortie.png [largeur]
juce_add_console_app(orbe808_snapshot PRODUCT_NAME "orbe808_snapshot")
target_sources(orbe808_snapshot PRIVATE ${O808_DIR}/snapshot_orbe808.cpp ${O808_DIR}/Orbe808Processor.cpp ${O808_DIR}/Orbe808Editor.cpp)
set_target_properties(orbe808_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbe808_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbe808_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
