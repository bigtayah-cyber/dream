include(${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/OrbeAnalysis/plugin.cmake)

juce_add_plugin(OrbeMix
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbemix
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OMix
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Mix"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Analyzer Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeMix PRIVATE ${ORBE_AN_SRC}
    Source/Suite/OrbeMix/MixProcessor.cpp Source/Suite/OrbeMix/MixEditor.cpp)
target_compile_definitions(OrbeMix PUBLIC ${ORBE_SUITE_DEFS})
target_link_libraries(OrbeMix PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
