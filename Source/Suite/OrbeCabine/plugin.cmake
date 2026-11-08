# Orbe Cabine : chaîne vocale par piste (VST3 + AU). Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine.
set(_cabdir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeCabine
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbecabine
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OCab
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Cabine"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeCabine PRIVATE ${_cabdir}/CabineProcessor.cpp ${_cabdir}/CabineEditor.cpp)
target_compile_definitions(OrbeCabine PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeCabine PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur (sans JUCE) : cmake --build . --target orbecabine_test
add_executable(orbecabine_test EXCLUDE_FROM_ALL ${_cabdir}/cabine_test.cpp)

# Contrôle visuel : cmake --build . --target orbecabine_snapshot (sous xvfb)
juce_add_console_app(orbecabine_snapshot PRODUCT_NAME "orbecabine_snapshot")
target_sources(orbecabine_snapshot PRIVATE ${_cabdir}/cabine_snapshot.cpp ${_cabdir}/CabineProcessor.cpp ${_cabdir}/CabineEditor.cpp)
set_target_properties(orbecabine_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbecabine_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbecabine_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
