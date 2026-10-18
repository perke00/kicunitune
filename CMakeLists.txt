cmake_minimum_required(VERSION 3.22)
project(KitsuneAutoTune VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MSVC)
  add_compile_options(/utf-8 /D_USE_MATH_DEFINES)
endif()

option(KITSUNE_BUILD_PLUGIN "Build the JUCE VST3 plugin (downloads JUCE)" ON)
option(KITSUNE_BUILD_TESTS  "Build the JUCE-free DSP tests" ON)

# ---- JUCE-free DSP library (also used by the unit tests) ----
add_library(KitsuneDSP STATIC
  Source/DSP/PitchDetector.cpp
  Source/DSP/PitchCorrector.cpp
  Source/DSP/PitchShifter.cpp
  Source/DSP/PitchEngine.cpp
  Source/DSP/DynamicResonanceReducer.cpp
  Source/DSP/ToneProcessor.cpp)
target_include_directories(KitsuneDSP PUBLIC Source)
if(NOT MSVC)
  target_compile_options(KitsuneDSP PRIVATE -Wall -Wextra)
endif()

if(KITSUNE_BUILD_TESTS)
  enable_testing()
  add_executable(KitsuneDSPTests Tests/DSPTests.cpp)
  target_link_libraries(KitsuneDSPTests PRIVATE KitsuneDSP)
  add_test(NAME DSP COMMAND KitsuneDSPTests)
endif()

if(KITSUNE_BUILD_PLUGIN)
  include(FetchContent)
  # JUCE is licensed AGPLv3 or commercial - see README "Licensing".
  FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(juce)

  juce_add_plugin(KitsuneAutoTune
    COMPANY_NAME "prodbyperke"
    PRODUCT_NAME "KITSUNE Auto Tune"
    PLUGIN_MANUFACTURER_CODE Kpbp
    PLUGIN_CODE Ktat
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE)

  juce_generate_juce_header(KitsuneAutoTune)

  target_sources(KitsuneAutoTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets/PresetManager.cpp)

  target_compile_definitions(KitsuneAutoTune PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

  target_link_libraries(KitsuneAutoTune
    PRIVATE KitsuneDSP juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

  # JUCE-based plugin tests (state, presets, latency, finite output)
  if(KITSUNE_BUILD_TESTS)
    juce_add_console_app(KitsunePluginTests PRODUCT_NAME "KitsunePluginTests")
    juce_generate_juce_header(KitsunePluginTests)
    target_sources(KitsunePluginTests PRIVATE
      Tests/PluginTests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets/PresetManager.cpp)
    target_compile_definitions(KitsunePluginTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(KitsunePluginTests PRIVATE KitsuneDSP juce::juce_audio_utils juce::juce_recommended_config_flags)
    add_test(NAME Plugin COMMAND KitsunePluginTests)
  endif()
endif()
