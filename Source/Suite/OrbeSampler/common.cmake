# Moteur partagé par Orbe Pads et Orbe Sample : déclaré une seule fois, chaque plugin ajoute ORBE_SAMPLER_SRC à ses sources.
include_guard(GLOBAL)
set(ORBE_SAMPLER_DIR ${CMAKE_CURRENT_LIST_DIR})
set(ORBE_SAMPLER_SRC ${ORBE_SAMPLER_DIR}/SamplerCommon.cpp)
# Test console du moteur (sans JUCE) : cmake --build . --target orbe_sampler_test
add_executable(orbe_sampler_test EXCLUDE_FROM_ALL ${ORBE_SAMPLER_DIR}/sampler_test.cpp)
