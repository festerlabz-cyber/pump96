cmake_minimum_required(VERSION 3.22)

project(Pump96 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is pulled in at configure time (pinned release). Pass -DJUCE_DIR=/path/to/JUCE to use a local checkout instead.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Formats: AU for Logic Pro, VST3 for other hosts. Override with -DPM96_FORMATS="VST3" (e.g. on Linux).
if(NOT DEFINED PM96_FORMATS)
    if(APPLE)
        set(PM96_FORMATS AU VST3)
    else()
        set(PM96_FORMATS VST3)
    endif()
endif()

juce_add_plugin(Pump96
    COMPANY_NAME "BoomTown Beats"
    COMPANY_WEBSITE "https://boomtownbeats.com"
    BUNDLE_ID com.boomtownbeats.pump96
    PLUGIN_MANUFACTURER_CODE Btbt
    PLUGIN_CODE Pm96
    PRODUCT_NAME "Pump 96"
    DESCRIPTION "Tempo-synced volume shaper"
    FORMATS ${PM96_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(Pump96Data SOURCES
    Resources/logo.png
    Resources/fonts/Anton-Regular.ttf
    Resources/fonts/Inter-SemiBold.ttf
    Resources/fonts/Inter-Bold.ttf
    Resources/fonts/JetBrainsMono-Medium.ttf)

target_sources(Pump96 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Pump96 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Pump96
    PRIVATE
        Pump96Data
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional local test tool (not used by the release build): -DPM96_BUILD_TESTS=ON
option(PM96_BUILD_TESTS "Build the offline render test" OFF)
if(PM96_BUILD_TESTS)
    juce_add_console_app(Pump96RenderTest PRODUCT_NAME "Pump96RenderTest")
    target_sources(Pump96RenderTest PRIVATE tests/RenderTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(Pump96RenderTest PRIVATE JucePlugin_Name="Pump 96" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(Pump96RenderTest PRIVATE Pump96Data juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
