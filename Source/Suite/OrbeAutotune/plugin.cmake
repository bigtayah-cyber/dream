# Orbe Autotune : correction de hauteur temps réel (VST3 + AU). Inclus par la boucle Source/Suite/*/plugin.cmake du CMakeLists racine.
set(_atudir ${CMAKE_CURRENT_LIST_DIR})
juce_add_plugin(OrbeAutotune
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbeautotune
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE OTun
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe Autotune"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx PitchShift
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(OrbeAutotune PRIVATE ${_atudir}/AutotuneProcessor.cpp ${_atudir}/AutotuneEditor.cpp)
target_compile_definitions(OrbeAutotune PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(OrbeAutotune PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test console du moteur (sans JUCE) : cmake --build . --target orbeautotune_test
add_executable(orbeautotune_test EXCLUDE_FROM_ALL ${_atudir}/autotune_test.cpp)

# Moteur « Orbe » (TuneCore.h) : mesures objectives et comparaison avec l'ancien moteur : cmake --build . --target tune_core_test
add_executable(tune_core_test EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/tests/tune_core_test.cpp)
target_compile_options(tune_core_test PRIVATE -O2)

# Contrôle visuel : cmake --build . --target orbeautotune_snapshot (sous xvfb)
juce_add_console_app(orbeautotune_snapshot PRODUCT_NAME "orbeautotune_snapshot")
target_sources(orbeautotune_snapshot PRIVATE ${_atudir}/autotune_snapshot.cpp ${_atudir}/AutotuneProcessor.cpp ${_atudir}/AutotuneEditor.cpp)
set_target_properties(orbeautotune_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbeautotune_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbeautotune_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
# Rendu « Satin » : modulation parasite, justesse, peigne Mix < 100 % : cmake --build . --target satin_test
add_executable(satin_test EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/tests/satin_test.cpp)
target_compile_options(satin_test PRIVATE -O2)
# Comportement (presets, peigne, attaque, warble, stabilisateur, formants en descente) : cmake --build . --target orbeautotune_behavior_test
add_executable(orbeautotune_behavior_test EXCLUDE_FROM_ALL ${_atudir}/behavior_test.cpp)
target_compile_options(orbeautotune_behavior_test PRIVATE -O2)
