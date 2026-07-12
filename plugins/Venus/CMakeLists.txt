cmake_minimum_required(VERSION 3.22)

project(VenusPlugin VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE
#
# By default JUCE is downloaded via FetchContent.  If you already have a JUCE
# checkout, point at it to skip the download:
#   cmake -B build -DJUCE_PATH=/path/to/JUCE
# To pin a different JUCE version when downloading:
#   cmake -B build -DJUCE_TAG=8.0.6
# ---------------------------------------------------------------------------
set(JUCE_PATH "" CACHE PATH   "Path to a local JUCE checkout (skips the download)")
set(JUCE_TAG  "8.0.4" CACHE STRING "JUCE git tag to fetch when JUCE_PATH is not set")

if(JUCE_PATH)
    message(STATUS "Venus: using local JUCE at ${JUCE_PATH}")
    add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    message(STATUS "Venus: fetching JUCE ${JUCE_TAG} via FetchContent")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
set(VENUS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VENUS_FORMATS AU)
endif()

juce_add_plugin(Venus
    PRODUCT_NAME               "Venus"
    COMPANY_NAME               "GuitarML"
    BUNDLE_ID                  com.guitarml.venus
    PLUGIN_MANUFACTURER_CODE   GmlA
    PLUGIN_CODE                Vnus
    FORMATS                    ${VENUS_FORMATS}
    VST3_CATEGORIES            Fx Reverb
    AU_MAIN_TYPE               kAudioUnitType_Effect
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE)

juce_generate_juce_header(Venus)

target_sources(Venus PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/VenusEngine.cpp)

target_include_directories(Venus PRIVATE Source)

target_compile_definitions(Venus PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Venus
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
