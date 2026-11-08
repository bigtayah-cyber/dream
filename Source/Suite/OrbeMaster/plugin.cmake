# Orbe Master : chaîne de mastering stéréo (VST3 + AU). Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine.
set(_masdir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeMaster
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbemaster
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OMas
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Master"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeMaster PRIVATE ${_masdir}/MasterProcessor.cpp ${_masdir}/MasterEditor.cpp)
target_compile_definitions(OrbeMaster PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeMaster PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur (sans JUCE) : cmake --build . --target orbemaster_test
add_executable(orbemaster_test EXCLUDE_FROM_ALL ${_masdir}/master_test.cpp)

# Contrôle visuel : cmake --build . --target orbemaster_snapshot (sous xvfb)
juce_add_console_app(orbemaster_snapshot PRODUCT_NAME "orbemaster_snapshot")
target_sources(orbemaster_snapshot PRIVATE ${_masdir}/master_snapshot.cpp ${_masdir}/MasterProcessor.cpp ${_masdir}/MasterEditor.cpp)
set_target_properties(orbemaster_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbemaster_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbemaster_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
