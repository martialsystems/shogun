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

set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../engine)

add_library(shogun_engine STATIC ${ENGINE_DIR}/shogun.cpp)
target_include_directories(shogun_engine PUBLIC ${ENGINE_DIR})
target_compile_options(shogun_engine PRIVATE -Wall -Wextra -Werror)
set_target_properties(shogun_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(SHOGUN_PLUGIN_SOURCES
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

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
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

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
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
