cmake_minimum_required(VERSION 3.15)
project(THDAnalyzerPlugin VERSION 1.0.0)

# Find JUCE - assume it's installed or use FetchContent
find_package(JUCE REQUIRED CONFIG)

juce_add_plugin(THDAnalyzerPlugin
    VERSION 1.0.0
    COMPANY_NAME "THD Analyzer"
    COMPANY_WEBSITE "https://github.com/thd-analyzer"
    
    # VST3 settings
    VST3_CAN_REPLACE_VST2 1
    VST3_COPY_EXTRAS 1
    
    # Plugin settings
    PLUGIN_MANUFACTURER_CODE "THDA"
    PLUGIN_CODE "THAN"
    PLUGIN_TYPE "ANALYSER"
    
    # Audio settings
    NEEDS_MIDI_INPUT 0
    NEEDS_MIDI_OUTPUT 0
    IS_MIDI_EFFECT 0
    IS_SYNTH 0
    
    # Channel configurations
    PLUGIN_CHANNEL_CONFIGURATIONS "{1,1},{2,2},{8,8}"
)

# Add source files
target_sources(THDAnalyzerPlugin PRIVATE
    Source/THDAnalyzerPlugin.cpp
)

# Add include directories
target_include_directories(THDAnalyzerPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# JUCE modules needed
juce_use_module_definitions(THDAnalyzerPlugin
    juce_core
    juce_audio_basics
    juce_audio_devices
    juce_audio_formats
    juce_audio_processors
    juce_audio_utils
    juce_core
    juce_data_structures
    juce_events
    juce_graphics
    juce_gui_basics
    juce_gui_extra
    juce_maths
    juce_spinners
    juce xylophone
)
