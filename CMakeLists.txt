cmake_minimum_required(VERSION 3.22)

# These two must come before project(), or the plugins are built for the macOS of the build machine.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
endif()

project(DrSeggUencer VERSION 0.1.10 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DRSEGG_BUILD_AAX "Also build AAX (Pro Tools; needs PACE signing to load in release Pro Tools)" OFF)
option(DRSEGG_BUILD_TESTS "Build the headless test runner" OFF)
option(DRSEGG_COPY_AFTER_BUILD "Install the plugins into the user plug-in folders after building" ON)
set(DRSEGG_VST2_SDK "" CACHE PATH "Steinberg VST2 SDK folder: if set, a VST2 is built too (the SDK is no longer distributed)")
set(DRSEGG_AAX_COPY_DIR "" CACHE PATH "Where the AAX is copied after building (empty = JUCE default, the system Avid folder, needs write access)")

# ---- JUCE: -DJUCE_DIR=<path>, or ./JUCE, or downloaded automatically (needs git)
set(DRSEGG_JUCE_TAG "8.0.10" CACHE STRING "JUCE version fetched when no local copy is given")
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
    set(JUCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/JUCE)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${DRSEGG_JUCE_TAG}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(JUCE)
    set(JUCE_DIR ${juce_SOURCE_DIR})
endif()

set(DRSEGG_FORMATS VST3)
if(APPLE)
    list(APPEND DRSEGG_FORMATS AU)
endif()
if(DRSEGG_VST2_SDK)
    juce_set_vst2_sdk_path(${DRSEGG_VST2_SDK})
    list(APPEND DRSEGG_FORMATS VST)
endif()
if(DRSEGG_BUILD_AAX)
    if(EXISTS ${JUCE_DIR}/modules/juce_audio_plugin_client/AAX/SDK)
        juce_set_aax_sdk_path(${JUCE_DIR}/modules/juce_audio_plugin_client/AAX/SDK)
    endif()
    list(APPEND DRSEGG_FORMATS AAX)
endif()

set(DRSEGG_EXTRA_PLUGIN_ARGS)
if(DRSEGG_AAX_COPY_DIR)
    list(APPEND DRSEGG_EXTRA_PLUGIN_ARGS AAX_COPY_DIR "${DRSEGG_AAX_COPY_DIR}")
endif()

# A sequencer that produces only MIDI, built as an instrument: Ableton Live refuses plugins flagged as MIDI effects.
# Stereo output (silent) is required by the hosts for an instrument.
juce_add_plugin(DrSeggUencer
    ${DRSEGG_EXTRA_PLUGIN_ARGS}
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Gianluca Sofia"
    BUNDLE_ID "com.gianlucasofia.drsegguencer"
    PLUGIN_MANUFACTURER_CODE Gsof
    PLUGIN_CODE Dseg
    PRODUCT_NAME "DR. SEGG-UENCER"
    FORMATS ${DRSEGG_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${DRSEGG_COPY_AFTER_BUILD}
    VST3_CATEGORIES Instrument
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    AAX_CATEGORY AAX_ePlugInCategory_SWGenerators)

# Embedded UI fonts (Rubik, Silkscreen; SIL Open Font License, see Resources/fonts) and the logo mark
juce_add_binary_data(DrSeggUencerFonts HEADER_NAME SeggFonts.h NAMESPACE SeggFonts SOURCES
    Resources/logo/drsegg-mark.svg
    Resources/fonts/Rubik-Medium.ttf
    Resources/fonts/Rubik-Bold.ttf
    Resources/fonts/Rubik-BlackItalic.ttf
    Resources/fonts/Silkscreen-Regular.ttf)
set_target_properties(DrSeggUencerFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(DrSeggUencer PRIVATE Source/SeggProcessor.cpp)
target_include_directories(DrSeggUencer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_definitions(DrSeggUencer PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    DRSEGG_VERSION="${PROJECT_VERSION}")
target_link_libraries(DrSeggUencer
    PRIVATE DrSeggUencerFonts juce::juce_audio_utils juce::juce_audio_processors juce::juce_audio_devices juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(DRSEGG_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
