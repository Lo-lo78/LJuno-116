cmake_minimum_required(VERSION 3.22)

project(LJuno116 VERSION 0.13.7 LANGUAGES C CXX)

# Keep the Windows plug-in self-contained: all project and JUCE targets use
# the static MSVC C/C++ runtime instead of requiring the VC Redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(LJuno116PresetData
    SOURCES
        "LJuno-116.jsfx.rpl"
        "Help/LJuno116Help.html"
)

juce_add_plugin(LJuno116
    COMPANY_NAME "Lo-lo"
    BUNDLE_ID "org.lolo.ljuno116"
    PLUGIN_MANUFACTURER_CODE LoLo
    PLUGIN_CODE Lj16
    FORMATS VST3
    PRODUCT_NAME "LJuno-116"
    DESCRIPTION "Accessible polyphonic synthesizer"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(LJuno116 PRIVATE
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/ParameterCatalog.cpp
    Source/ParameterCatalog.h
    Source/PresetManager.cpp
    Source/PresetManager.h
    Source/SynthEngine.cpp
    Source/SynthEngine.h
    Source/ScreenReaderAnnouncer.cpp
    Source/ScreenReaderAnnouncer.h
    Source/GeneratedParameters.h
    Source/GeneratedPages.h
)

target_compile_features(LJuno116 PRIVATE cxx_std_20)

target_compile_definitions(LJuno116
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LJuno116
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        LJuno116PresetData
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(WIN32)
    target_link_libraries(LJuno116 PRIVATE Uiautomationcore OleAut32)
endif()

option(LJUNO_BUILD_SMOKE_TEST "Build the offline DSP smoke test" ON)

if(LJUNO_BUILD_SMOKE_TEST)
    juce_add_console_app(LJuno116SmokeTest PRODUCT_NAME "LJuno-116 DSP Smoke Test")
    target_sources(LJuno116SmokeTest PRIVATE Tests/SynthSmokeTest.cpp)
    target_compile_features(LJuno116SmokeTest PRIVATE cxx_std_20)
    target_link_libraries(LJuno116SmokeTest PRIVATE LJuno116 juce::juce_audio_utils)
endif()
