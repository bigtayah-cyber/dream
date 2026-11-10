# Orbe Galet : mise en forme par le temps (forme dessinée → volume, filtre, distorsion lisse, pan) (VST3 + AU).
# Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine. Moteur sans JUCE : GaletDsp.h.
set(_galdir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeGalet
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbegalet
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OGal
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Galet"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Modulation
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeGalet PRIVATE ${_galdir}/GaletProcessor.cpp ${_galdir}/GaletEditor.cpp)
target_compile_definitions(OrbeGalet PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeGalet PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur (sans JUCE) : cmake --build . --target orbegalet_test
add_executable(orbegalet_test EXCLUDE_FROM_ALL ${_galdir}/galet_test.cpp)

# Contrôle visuel : cmake --build . --target orbegalet_snapshot (sous xvfb)
juce_add_console_app(orbegalet_snapshot PRODUCT_NAME "orbegalet_snapshot")
target_sources(orbegalet_snapshot PRIVATE ${_galdir}/galet_snapshot.cpp ${_galdir}/GaletProcessor.cpp ${_galdir}/GaletEditor.cpp)
set_target_properties(orbegalet_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbegalet_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbegalet_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
