cmake_minimum_required(VERSION 3.22)

# NOTE: The source tree path contains an '&' character (Calm&Tender).
# The JUCE VST3 post-build signing script (checkBundleSigning.cmake) breaks
# when the build directory also contains '&'. Always configure with a build
# directory outside the source tree, e.g.:
#   cmake -S plugin -B /tmp/fmma_build -DMIX_ANALYZER_JUCE_DIR=<path>
# or set CMAKE_BINARY_DIR to a safe path before running cmake.

project(FunkyMooseMixAnalyzerPlugin VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

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
        Source/PluginEditor.cpp
        Source/UI/SummaryComponent.cpp
        Source/UI/MetricsCardComponent.cpp
        Source/UI/ToneShapeComponent.cpp
        Source/UI/ActionChecklistComponent.cpp
        Source/UI/ScoreComponent.cpp
        Source/UI/CompareComponent.cpp)

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

if(BUILD_TESTING)
    add_executable(FunkyMooseMixAnalyzerTests
        Source/AnalysisModel.cpp
        Tests/AnalysisModelTests.cpp)

    target_include_directories(FunkyMooseMixAnalyzerTests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source")

    target_compile_definitions(FunkyMooseMixAnalyzerTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0)

    target_link_libraries(FunkyMooseMixAnalyzerTests
        PRIVATE
            juce::juce_core
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME FunkyMooseMixAnalyzerTests COMMAND FunkyMooseMixAnalyzerTests)

    add_executable(FunkyMooseMixAnalyzerAudioFixtureTests
        Source/AnalysisModel.cpp
        Source/PluginProcessor.cpp
        Tests/AudioFixtureTests.cpp)

    target_include_directories(FunkyMooseMixAnalyzerAudioFixtureTests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source")

    target_compile_definitions(FunkyMooseMixAnalyzerAudioFixtureTests
        PRIVATE
            MIX_ANALYZER_HEADLESS_TESTS=1
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(FunkyMooseMixAnalyzerAudioFixtureTests
        PRIVATE
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME FunkyMooseMixAnalyzerAudioFixtureTests COMMAND FunkyMooseMixAnalyzerAudioFixtureTests)

    add_custom_target(FunkyMooseMixAnalyzerAllTests
        DEPENDS
            FunkyMooseMixAnalyzerTests
            FunkyMooseMixAnalyzerAudioFixtureTests)
endif()
