cmake_minimum_required(VERSION 3.22)
project(KERV VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded automatically, unless you point FETCHCONTENT_SOURCE_DIR_JUCE at a local copy.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Windows/Linux: VST3 + app.  Mac: AU (Logic, GarageBand...) + VST3 + app, Intel and Apple Silicon.
set(KERV_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND KERV_FORMATS AU)
endif()

juce_add_plugin(KERV
    COMPANY_NAME                "kimyondabeat"
    PRODUCT_NAME                "KERV"
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    PLUGIN_MANUFACTURER_CODE    Kimy
    PLUGIN_CODE                 Kerv
    FORMATS                     ${KERV_FORMATS}
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    BUNDLE_ID                   com.kimyondabeat.kerv
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Drum
    COPY_PLUGIN_AFTER_BUILD     FALSE)

# The drum kit: every file in Kit/ is embedded in the plugin.
# Names must look like  <category>__<NN>__<name>.flac  (made by tools/stage_library.py).
file(GLOB KERV_KIT_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Kit/*")
juce_add_binary_data(KervKit
    HEADER_NAME BinaryData.h
    NAMESPACE   BinaryData
    SOURCES     ${KERV_KIT_FILES})

target_sources(KERV PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Panels.cpp
    Source/KitView.cpp
    Source/Library.cpp
    Source/Presets.cpp)

target_compile_definitions(KERV PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(KERV
    PRIVATE
        KervKit
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
