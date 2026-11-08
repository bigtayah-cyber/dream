include(${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/OrbeAnalysis/plugin.cmake)

juce_add_plugin(OrbeFredonne
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbefredonne
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OFre
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Fredonne"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    VST3_CATEGORIES Instrument
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeFredonne PRIVATE ${ORBE_AN_SRC}
    Source/Suite/OrbeFredonne/FredonneProcessor.cpp Source/Suite/OrbeFredonne/FredonneEditor.cpp)
target_compile_definitions(OrbeFredonne PUBLIC ${ORBE_SUITE_DEFS})
target_link_libraries(OrbeFredonne PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
