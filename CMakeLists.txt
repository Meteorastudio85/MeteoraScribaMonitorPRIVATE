cmake_minimum_required(VERSION 3.22)

project(ScribaMonitor VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# JUCE: usa una copia locale (cartella JUCE accanto al progetto) se presente,
# altrimenti la scarica da GitHub.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(SCRIBA_BUILD_TESTS "Compila il test del motore DSP" OFF)

juce_add_plugin(ScribaMonitor
    COMPANY_NAME "Meteora Plugins"
    COMPANY_EMAIL "meteora_plugins@aol.com"
    BUNDLE_ID "com.meteoraplugins.scribamonitor"
    PLUGIN_MANUFACTURER_CODE Mtra
    PLUGIN_CODE ScMn
    PRODUCT_NAME "Scriba Monitor"
    PLUGIN_NAME "Scriba Monitor (Meteora Plugins)"
    DESCRIPTION "Correzione acustica della stanza per monitor da studio"
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Scriba Monitor usa il microfono di misura per analizzare l'acustica della stanza.")

juce_add_binary_data(ScribaMonitorData SOURCES
    Resources/ECM8000_generic.cal)

set(SCRIBA_DSP_SOURCES
    Source/dsp/FrequencyResponse.cpp
    Source/dsp/MicCalibration.cpp
    Source/dsp/SweepAnalyzer.cpp
    Source/dsp/FilterDesigner.cpp
    Source/dsp/Profile.cpp)

target_sources(ScribaMonitor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MeasurementSession.cpp
    Source/gui/ResponseGraph.cpp
    Source/gui/MeasurePage.cpp
    Source/gui/CorrectionPage.cpp
    ${SCRIBA_DSP_SOURCES})

target_include_directories(ScribaMonitor PRIVATE Source)

set(SCRIBA_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(SCRIBA_LINUX_MINIMAL)
    # Solo per build di verifica su Linux senza le librerie di sviluppo X/ALSA.
    list(APPEND SCRIBA_DEFINES JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
                               JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_compile_definitions(ScribaMonitor PUBLIC ${SCRIBA_DEFINES})

target_link_libraries(ScribaMonitor
    PRIVATE
        ScribaMonitorData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(SCRIBA_BUILD_TESTS)
    juce_add_console_app(ScribaDspTest PRODUCT_NAME "ScribaDspTest")
    target_sources(ScribaDspTest PRIVATE tests/DspTest.cpp ${SCRIBA_DSP_SOURCES})
    target_include_directories(ScribaDspTest PRIVATE Source)
    target_compile_definitions(ScribaDspTest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 SCRIBA_NO_BINARY_DATA=1)
    target_link_libraries(ScribaDspTest PRIVATE juce::juce_core juce::juce_audio_basics juce::juce_dsp
                          PUBLIC juce::juce_recommended_config_flags)
endif()
