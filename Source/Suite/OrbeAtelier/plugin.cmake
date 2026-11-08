# Orbe Atelier : console vocale modulaire, presets = looks d'Orbe Looks (VST3 + AU). Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine.
set(_atedir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeAtelier
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbeatelier
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OAte
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Atelier"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeAtelier PRIVATE ${_atedir}/AtelierProcessor.cpp ${_atedir}/AtelierEditor.cpp)
target_compile_definitions(OrbeAtelier PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeAtelier PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags PUBLIC juce::juce_recommended_lto_flags)

# Contrôle visuel : cmake --build . --target orbeatelier_snapshot (sous xvfb)
juce_add_console_app(orbeatelier_snapshot PRODUCT_NAME "orbeatelier_snapshot")
target_sources(orbeatelier_snapshot PRIVATE ${_atedir}/atelier_snapshot.cpp ${_atedir}/AtelierProcessor.cpp ${_atedir}/AtelierEditor.cpp)
set_target_properties(orbeatelier_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbeatelier_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbeatelier_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
