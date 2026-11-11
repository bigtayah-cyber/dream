include(${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/OrbeAnalysis/plugin.cmake)

juce_add_plugin(OrbeDetect
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbedetect
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE ODet
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Detect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Analyzer Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeDetect PRIVATE ${ORBE_AN_SRC}
    Source/Suite/OrbeDetect/DetectProcessor.cpp Source/Suite/OrbeDetect/DetectEditor.cpp)
target_compile_definitions(OrbeDetect PUBLIC ${ORBE_SUITE_DEFS})
target_link_libraries(OrbeDetect PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console (sans JUCE) : carte des fausses notes et collage de tonalité. cmake --build . --target orbedetect_test
add_executable(orbedetect_test EXCLUDE_FROM_ALL Source/Suite/OrbeDetect/falsenotes_test.cpp ${ORBE_AN_SRC})
