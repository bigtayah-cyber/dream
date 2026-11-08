# Orbe Denoiser : réducteur de bruit voix (VST3 + AU). Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine.
# Briques partagées (DSP + face avant) : Source/Suite/OrbeDsp/*.h, header-only, rien à déclarer.
set(_dendir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeDenoiser
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbedenoiser
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE ODen
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Denoiser"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Restoration
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeDenoiser PRIVATE ${_dendir}/DenoiserProcessor.cpp ${_dendir}/DenoiserEditor.cpp)
target_compile_definitions(OrbeDenoiser PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeDenoiser PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur (sans JUCE) : cmake --build . --target orbedenoiser_test
add_executable(orbedenoiser_test EXCLUDE_FROM_ALL ${_dendir}/denoiser_test.cpp)

# Contrôle visuel : cmake --build . --target orbedenoiser_snapshot (sous xvfb)
juce_add_console_app(orbedenoiser_snapshot PRODUCT_NAME "orbedenoiser_snapshot")
target_sources(orbedenoiser_snapshot PRIVATE ${_dendir}/denoiser_snapshot.cpp ${_dendir}/DenoiserProcessor.cpp ${_dendir}/DenoiserEditor.cpp)
set_target_properties(orbedenoiser_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbedenoiser_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbedenoiser_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
