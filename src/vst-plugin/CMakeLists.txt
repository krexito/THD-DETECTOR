cmake_minimum_required(VERSION 3.22)
project(THDAnalyzerPlugin VERSION 1.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/JUCE" JUCE)
else()
    find_package(JUCE REQUIRED CONFIG)
endif()

juce_add_plugin(THDAnalyzerPlugin
    COMPANY_NAME "THD Analyzer"
    COMPANY_WEBSITE "https://github.com/thd-analyzer"
    BUNDLE_ID "com.thdanalyzer.vst3"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE THDA
    PLUGIN_CODE THAN
    FORMATS VST3
    PRODUCT_NAME "THD - TotalHarmonicDisplay"
    PLUGIN_CHANNEL_CONFIGURATIONS "{1,1},{2,2},{6,6},{8,8}"
)

target_sources(THDAnalyzerPlugin
    PRIVATE
        Source/THDAnalyzerPlugin.cpp
)

target_include_directories(THDAnalyzerPlugin
    PRIVATE
        Source
)

target_compile_definitions(THDAnalyzerPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(THDAnalyzerPlugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
