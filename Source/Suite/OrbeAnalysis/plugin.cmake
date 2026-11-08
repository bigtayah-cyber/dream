# Moteur d'analyse partagé de la suite Orbe (C++17 sans JUCE) : déclaré une seule fois.
# Chaque plugin de la suite ajoute ORBE_AN_SRC à ses sources.
include_guard(GLOBAL)
set(ORBE_AN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/OrbeAnalysis)
set(ORBE_AN_SRC ${ORBE_AN_DIR}/OrbeAnalysis.cpp)
set(ORBE_SUITE_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

# Tests console : cmake --build <dir> --target orbe_analysis_test && <dir>/orbe_analysis_test
add_executable(orbe_analysis_test EXCLUDE_FROM_ALL ${ORBE_AN_DIR}/AnalysisTest.cpp ${ORBE_AN_SRC})

# Contrôle visuel des trois éditeurs (sous xvfb) : orbe_suite_snapshot detect|mix|fredonne sortie.png signal.wav [largeur]
juce_add_console_app(orbe_suite_snapshot PRODUCT_NAME "orbe_suite_snapshot")
target_sources(orbe_suite_snapshot PRIVATE ${ORBE_AN_DIR}/SuiteSnapshot.cpp ${ORBE_AN_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/OrbeDetect/DetectProcessor.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/OrbeDetect/DetectEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/OrbeMix/MixProcessor.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/OrbeMix/MixEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/OrbeFredonne/FredonneProcessor.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/OrbeFredonne/FredonneEditor.cpp)
set_target_properties(orbe_suite_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbe_suite_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_STANDALONE_APPLICATION=1 ORBE_SUITE_SNAPSHOT=1 ${ORBE_SUITE_DEFS})
target_link_libraries(orbe_suite_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
