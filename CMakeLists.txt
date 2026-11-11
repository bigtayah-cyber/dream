cmake_minimum_required(VERSION 3.22)
project(dreamaws VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

if(MSVC)
    add_compile_options(/utf-8 /bigobj /Zc:__cplusplus)
endif()

include(FetchContent)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

if(APPLE)
    set(VOILE_FORMATS VST3 AU)
else()
    set(VOILE_FORMATS VST3)
endif()

juce_add_plugin(Voile
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.voile
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE Voil
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Voile"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Voile PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Voile PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Voile
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Test hors-ligne du moteur (sans JUCE) : cmake --build . --target voile_offline_test
add_executable(voile_offline_test EXCLUDE_FROM_ALL tests/offline.cpp)

# ---------------- Orbe (générateur d'accords, de lead et de basse ; MIDI) ----------------
juce_add_plugin(Orbe
    COMPANY_NAME "dreamaws"
    BUNDLE_ID com.dreamaws.orbe
    PLUGIN_MANUFACTURER_CODE Drmw
    PLUGIN_CODE Orbe
    FORMATS ${VOILE_FORMATS}
    PRODUCT_NAME "Orbe"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(Orbe PRIVATE Source/Orbe/OrbeProcessor.cpp Source/Orbe/OrbeEditor.cpp)
target_compile_definitions(Orbe PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Orbe PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Orbe FX : vrai effet audio (chaîne filtre / saturation / chorus / delay / réverbe) -> Source/Suite/OrbeFX/plugin.cmake
# Orbe Rythme : générateur de batterie MIDI, issu d'Orbe -> Source/Suite/OrbeRythme/plugin.cmake

add_executable(orbe_test EXCLUDE_FROM_ALL tests/orbe_test.cpp)

# Outil de contrôle visuel : cmake --build . --target orbe_snapshot (exécuter sous xvfb)
juce_add_console_app(orbe_snapshot PRODUCT_NAME "orbe_snapshot")
target_sources(orbe_snapshot PRIVATE tests/snapshot.cpp Source/Orbe/OrbeProcessor.cpp Source/Orbe/OrbeEditor.cpp)
set_target_properties(orbe_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbe_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbe_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

# Version d'état (Kit/StateVersion.h) : états anciens relus, identifiants intacts. cmake --build <dir> --target state_version_test
juce_add_console_app(state_version_test PRODUCT_NAME "state_version_test")
target_sources(state_version_test PRIVATE tests/state_version_test.cpp)
set_target_properties(state_version_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(state_version_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(state_version_test PRIVATE juce::juce_data_structures juce::juce_recommended_config_flags)

juce_add_console_app(orbe_audiotest PRODUCT_NAME "orbe_audiotest")
target_sources(orbe_audiotest PRIVATE tests/orbe_audio.cpp Source/Orbe/OrbeProcessor.cpp Source/Orbe/OrbeEditor.cpp)
set_target_properties(orbe_audiotest PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(orbe_audiotest PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(orbe_audiotest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

# ---------------- Plugins à un potard ----------------
function(add_oneknob NAME PRODUCT CODE IDX BUNDLE)
    juce_add_plugin(${NAME}
        COMPANY_NAME "dreamaws"
        BUNDLE_ID com.dreamaws.${BUNDLE}
        PLUGIN_MANUFACTURER_CODE Drmw
        PLUGIN_CODE ${CODE}
        FORMATS ${VOILE_FORMATS}
        PRODUCT_NAME "${PRODUCT}"
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES Fx
        AU_MAIN_TYPE kAudioUnitType_Effect
        COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(${NAME} PRIVATE Source/OneKnob/OneKnobProcessor.cpp Source/OneKnob/OneKnobEditor.cpp)
    target_compile_definitions(${NAME} PUBLIC ONEK_INDEX=${IDX} JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${NAME} PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
endfunction()

add_oneknob(GrainNord "Grain Nord" Gnrd 0 grainnord)
add_oneknob(Gorge "Gorge" Grge 1 gorge)
add_oneknob(Poupee "Poupee" Pupe 2 poupee)
add_oneknob(Patine "Patine" Ptin 3 patine)

add_executable(onek_test EXCLUDE_FROM_ALL tests/onek_test.cpp)

juce_add_console_app(onek_snapshot PRODUCT_NAME "onek_snapshot")
target_sources(onek_snapshot PRIVATE tests/snapshot_onek.cpp Source/OneKnob/OneKnobProcessor.cpp Source/OneKnob/OneKnobEditor.cpp)
set_target_properties(onek_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(onek_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(onek_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

juce_add_console_app(voile_snapshot PRODUCT_NAME "voile_snapshot")
target_sources(voile_snapshot PRIVATE tests/snapshot_voile.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
set_target_properties(voile_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(voile_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(voile_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

# Suite Orbe : chaque plugin vit dans Source/Suite/<Nom>/ avec son plugin.cmake
file(GLOB ORBE_SUITE_CMAKE CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/Suite/*/plugin.cmake)
foreach(f ${ORBE_SUITE_CMAKE})
    include(${f})
endforeach()

# ---------------- Kit graphique : test de la balistique du vumètre (sans JUCE) ----------------
# cmake --build . --target kit_vu_test && ./kit_vu_test
add_executable(kit_vu_test EXCLUDE_FROM_ALL tests/kit_vu_test.cpp)
# Planche de contrôle visuel du Kit (sous xvfb) : cmake --build . --target kit_board && xvfb-run -a ./kit_board_artefacts/Release/kit_board planche.png 2
juce_add_console_app(kit_board PRODUCT_NAME "kit_board")
target_sources(kit_board PRIVATE tests/kit_board.cpp)
set_target_properties(kit_board PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_definitions(kit_board PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(kit_board PRIVATE juce::juce_gui_basics juce::juce_recommended_config_flags)
