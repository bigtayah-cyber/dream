# Orbe Pads : sampler 16 pads (C1 à D#2). Moteur partagé dans ../OrbeSampler.
include(${CMAKE_CURRENT_LIST_DIR}/../OrbeSampler/common.cmake)
set(ORBE_PADS_SRC ${CMAKE_CURRENT_LIST_DIR}/PadsProcessor.cpp ${CMAKE_CURRENT_LIST_DIR}/PadsEditor.cpp ${ORBE_SAMPLER_SRC})

juce_add_plugin(OrbePads
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbepads
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OPad
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Pads"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbePads PRIVATE ${ORBE_PADS_SRC})
# JUCE_USE_MP3AUDIOFORMAT : décodeur MP3 de JUCE (sans lui, Windows et Linux ne lisent pas le MP3 ; Mac passe par CoreAudio)
target_compile_definitions(OrbePads PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(OrbePads PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Contrôle visuel : cmake --build . --target orbe_pads_snapshot (exécuter sous xvfb)
juce_add_console_app(orbe_pads_snapshot PRODUCT_NAME "orbe_pads_snapshot")
target_sources(orbe_pads_snapshot PRIVATE ${CMAKE_CURRENT_LIST_DIR}/snapshot.cpp ${ORBE_PADS_SRC})
set_target_properties(orbe_pads_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbe_pads_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbe_pads_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
