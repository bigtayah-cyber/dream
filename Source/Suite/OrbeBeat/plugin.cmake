# Orbe Beat : boîte à rythmes 8 pistes (VST3 + AU). Inclus par CMakeLists.txt (glob Source/Suite/*/plugin.cmake).
set(OBEAT_DIR ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeBeat
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbebeat
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OBet
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Beat"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeBeat PRIVATE ${OBEAT_DIR}/OrbeBeatProcessor.cpp ${OBEAT_DIR}/OrbeBeatEditor.cpp)
target_compile_definitions(OrbeBeat PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeBeat PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test du moteur sans JUCE : cmake --build . --target orbebeat_test && ./orbebeat_test
add_executable(orbebeat_test EXCLUDE_FROM_ALL ${OBEAT_DIR}/test_orbebeat.cpp)

# Contrôle visuel : cmake --build . --target orbebeat_snapshot, puis xvfb-run -a ./orbebeat_snapshot sortie.png [largeur] [aleatoire]
juce_add_console_app(orbebeat_snapshot PRODUCT_NAME "orbebeat_snapshot")
target_sources(orbebeat_snapshot PRIVATE ${OBEAT_DIR}/snapshot_orbebeat.cpp ${OBEAT_DIR}/OrbeBeatProcessor.cpp ${OBEAT_DIR}/OrbeBeatEditor.cpp)
set_target_properties(orbebeat_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbebeat_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbebeat_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
