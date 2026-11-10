# Orbe Sample : transformateur de sample jouable. Moteur partagé dans ../OrbeSampler.
include(${CMAKE_CURRENT_LIST_DIR}/../OrbeSampler/common.cmake)
set(ORBE_SAMPLE_SRC ${CMAKE_CURRENT_LIST_DIR}/SampleProcessor.cpp ${CMAKE_CURRENT_LIST_DIR}/SampleEditor.cpp ${ORBE_SAMPLER_SRC})

juce_add_plugin(OrbeSample
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbesample
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OSmp
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Sample"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeSample PRIVATE ${ORBE_SAMPLE_SRC})
# JUCE_USE_MP3AUDIOFORMAT : décodeur MP3 de JUCE (sans lui, Windows et Linux ne lisent pas le MP3 ; Mac passe par CoreAudio)
target_compile_definitions(OrbeSample PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(OrbeSample PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Contrôle visuel : cmake --build . --target orbe_sample_snapshot (exécuter sous xvfb)
juce_add_console_app(orbe_sample_snapshot PRODUCT_NAME "orbe_sample_snapshot")
target_sources(orbe_sample_snapshot PRIVATE ${CMAKE_CURRENT_LIST_DIR}/snapshot.cpp ${ORBE_SAMPLE_SRC})
set_target_properties(orbe_sample_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbe_sample_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbe_sample_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
