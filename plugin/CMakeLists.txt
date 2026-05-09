cmake_minimum_required(VERSION 3.22)

project(FunkyMooseMixAnalyzerPlugin VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MIX_ANALYZER_JUCE_DIR "$ENV{JUCE_DIR}" CACHE PATH "Path to a local JUCE checkout")
option(MIX_ANALYZER_COPY_PLUGIN_AFTER_BUILD "Copy built plugins into the user plugin folders" OFF)

if(MIX_ANALYZER_JUCE_DIR AND EXISTS "${MIX_ANALYZER_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${MIX_ANALYZER_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    find_package(JUCE CONFIG QUIET)
    if(NOT TARGET juce::juce_audio_processors)
        message(FATAL_ERROR
            "JUCE was not found. Configure with "
            "-DMIX_ANALYZER_JUCE_DIR=/path/to/JUCE or set JUCE_DIR.")
    endif()
endif()

set(MIX_ANALYZER_PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MIX_ANALYZER_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(FunkyMooseMixAnalyzer
    COMPANY_NAME "Funky Moose"
    BUNDLE_ID "com.funkymoose.mixanalyzer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${MIX_ANALYZER_COPY_PLUGIN_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Fnky
    PLUGIN_CODE FmMa
    FORMATS ${MIX_ANALYZER_PLUGIN_FORMATS}
    PRODUCT_NAME "Funky Moose Mix Analyzer")

juce_generate_juce_header(FunkyMooseMixAnalyzer)

target_sources(FunkyMooseMixAnalyzer
    PRIVATE
        Source/AnalysisModel.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(FunkyMooseMixAnalyzer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(FunkyMooseMixAnalyzer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
