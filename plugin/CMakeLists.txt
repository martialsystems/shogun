cmake_minimum_required(VERSION 3.22)
project(ShogunPlugin VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# Compiler options by compiler front end (GCC/Clang vs MSVC/clang-cl).
#  - Floating-point contraction (FMA fusion) is off for the engine, the plugin and the probe, so every platform and
#    instruction set renders the same samples (TESTPLAN "Floating-point contraction"). MSVC: /fp:precise (no
#    contraction by default since VS 2022); clang-cl also needs -ffp-contract=off, since its /fp:precise allows it.
#  - Warnings: our sources build with -Wall -Wextra -Werror on GCC/Clang. MSVC gets /W4 as warnings only (no /WX):
#    the engine and the JUCE headers it is compiled with have not been built warning-free under MSVC here.
if(MSVC)
  set(SHOGUN_FP_OPTIONS /fp:precise)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(APPEND SHOGUN_FP_OPTIONS /clang:-ffp-contract=off)
  endif()
  set(SHOGUN_WARN_OPTIONS /W4)
else()
  set(SHOGUN_FP_OPTIONS -ffp-contract=off)
  set(SHOGUN_WARN_OPTIONS -Wall -Wextra -Werror)
endif()

set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../engine)
# Shared Jidai headers (JCS + jidai::dsp), vendored: third_party/jidai-common/VENDORED.md.
set(JIDAI_COMMON_INC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/jidai-common/include)

add_library(shogun_engine STATIC ${ENGINE_DIR}/shogun.cpp)
target_include_directories(shogun_engine PUBLIC ${ENGINE_DIR} ${JIDAI_COMMON_INC})
target_compile_options(shogun_engine PRIVATE ${SHOGUN_WARN_OPTIONS} PUBLIC ${SHOGUN_FP_OPTIONS})
if(MSVC)
  target_compile_definitions(shogun_engine PRIVATE _CRT_SECURE_NO_WARNINGS)  # std::sscanf / strcpy in engine/
endif()
set_target_properties(shogun_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(SHOGUN_PLUGIN_SOURCES
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)
# Our sources build warning-clean (-Werror on GCC/Clang); JUCE's own modules keep JUCE's flags.
set_source_files_properties(${SHOGUN_PLUGIN_SOURCES} Source/Probe.cpp PROPERTIES COMPILE_OPTIONS "${SHOGUN_WARN_OPTIONS}")

juce_add_plugin(Shogun
  COMPANY_NAME "Martial Systems"
  BUNDLE_ID com.martialsystems.shogun
  PLUGIN_MANUFACTURER_CODE Mrsy
  PLUGIN_CODE Shgn
  FORMATS VST3
  PRODUCT_NAME "SHOGUN"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  VST3_CATEGORIES "Instrument|Synth"
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Shogun PRIVATE ${SHOGUN_PLUGIN_SOURCES})
target_include_directories(Shogun PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source ${ENGINE_DIR})
target_compile_definitions(Shogun PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Shogun
  PRIVATE
    shogun_engine
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags)
# PUBLIC: also reaches the format wrapper targets (Shogun_VST3) that link Shogun.
target_compile_options(Shogun PUBLIC ${SHOGUN_FP_OPTIONS})
# Warning policy is the project's -Wall -Wextra -Werror (set on our sources above), not JUCE's extended set
# (-Wsign-conversion, -Wfloat-equal …), which the engine does not target.

juce_add_console_app(ShogunProbe PRODUCT_NAME "ShogunProbe")
target_sources(ShogunProbe PRIVATE Source/Probe.cpp ${SHOGUN_PLUGIN_SOURCES})
target_include_directories(ShogunProbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source ${ENGINE_DIR})
target_compile_definitions(ShogunProbe PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(ShogunProbe
  PRIVATE
    shogun_engine
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)
target_compile_options(ShogunProbe PRIVATE ${SHOGUN_FP_OPTIONS})
