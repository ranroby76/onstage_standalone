cmake_minimum_required(VERSION 3.22)
project(Colosseum VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Platform Detection
# =============================================================================
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    message(STATUS "Building for Windows")
elseif(APPLE)
    set(PLATFORM_MAC TRUE)
    message(STATUS "Building for macOS")
elseif(UNIX)
    set(PLATFORM_LINUX TRUE)
    message(STATUS "Building for Linux")
endif()

# =============================================================================
# JUCE Path Configuration
# =============================================================================
if(PLATFORM_WINDOWS)
    set(JUCE_PATH "D:/Workspace/JUCE" CACHE PATH "Path to JUCE")
    set(ASIO_SDK_PATH "D:/Workspace/ASIOSDK" CACHE PATH "Path to ASIO SDK")
elseif(PLATFORM_MAC)
    set(JUCE_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to JUCE")
elseif(PLATFORM_LINUX)
    set(JUCE_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to JUCE")
endif()

# Optional SDK Paths (set these if you have the SDKs)
set(CLAP_SDK_PATH "" CACHE PATH "Path to CLAP SDK (optional)")
set(CLAP_WRAPPER_PATH "" CACHE PATH "Path to clap-wrapper for CLAP hosting (optional)")

# =============================================================================
# VST2 SDK - DISABLED (causes stability issues with unofficial SDKs)
# =============================================================================
set(VST2_SUPPORT FALSE)
message(STATUS "VST2 hosting: Disabled (stability reasons)")

add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)

# =============================================================================
# Check for Optional SDKs
# =============================================================================

# CLAP SDK Check
if(EXISTS "${CLAP_SDK_PATH}/include/clap/clap.h")
    set(CLAP_SDK_FOUND TRUE)
    message(STATUS "CLAP SDK found at: ${CLAP_SDK_PATH}")
else()
    set(CLAP_SDK_FOUND FALSE)
    message(STATUS "CLAP SDK not found (optional)")
endif()

# CLAP Wrapper Check (for hosting CLAP plugins)
if(EXISTS "${CLAP_WRAPPER_PATH}/CMakeLists.txt")
    set(CLAP_HOSTING_SUPPORT TRUE)
    add_subdirectory(${CLAP_WRAPPER_PATH} ${CMAKE_BINARY_DIR}/clap-wrapper)
    message(STATUS "CLAP hosting enabled via clap-wrapper")
else()
    set(CLAP_HOSTING_SUPPORT FALSE)
    message(STATUS "CLAP hosting disabled (clap-wrapper not found)")
endif()

# =============================================================================
# Standalone Application Target
# =============================================================================
# Note: Icon handling disabled due to JUCE CMake quirks
# Icons can be added post-build or via resource files
juce_add_gui_app(SubterraneumApp
    PRODUCT_NAME "Colosseum"
    COMPANY_NAME "Subcore"
    BUNDLE_ID "com.subcore.colosseum"
)

juce_generate_juce_header(SubterraneumApp)

# =============================================================================
# Source Files
# =============================================================================
target_sources(SubterraneumApp PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/UIComponents.cpp
    src/MidiSelectors.cpp
    src/MixerStrip.cpp
    src/MixerView.cpp
    src/PluginManagerTab.cpp
    src/AudioSettingsTab.cpp
    src/InstrumentSelector.cpp
    src/GraphCanvas.cpp
    src/StandaloneMain.cpp
    src/RegistrationManager.cpp
    src/SimpleConnectorProcessor.cpp
    src/StereoMeterProcessor.cpp
    src/StudioTab.cpp
    src/ManualTab.cpp
    src/RegistrationTab.cpp
)

# =============================================================================
# Windows Icon via Resource File (optional)
# =============================================================================
# To add an icon:
# 1. Place icon.ico in src/ folder
# 2. Place resources.rc in src/ folder with content:
#    IDI_ICON1 ICON "icon.ico"
if(PLATFORM_WINDOWS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/resources.rc")
        target_sources(SubterraneumApp PRIVATE "${CMAKE_SOURCE_DIR}/src/resources.rc")
        message(STATUS "Using resource file: src/resources.rc")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/resources.rc")
        target_sources(SubterraneumApp PRIVATE "${CMAKE_SOURCE_DIR}/resources.rc")
        message(STATUS "Using resource file: resources.rc")
    else()
        message(STATUS "No resource file found - building without custom icon")
    endif()
endif()

# =============================================================================
# Compile Definitions
# =============================================================================
set(COMMON_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    
    # Plugin Hosting - VST3 always enabled
    JUCE_PLUGINHOST_VST3=1
)

# --- Windows Definitions ---
if(PLATFORM_WINDOWS)
    list(APPEND COMMON_DEFINITIONS
        JUCE_ASIO=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=0
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
    )
endif()

# --- macOS Definitions ---
if(PLATFORM_MAC)
    list(APPEND COMMON_DEFINITIONS
        JUCE_ASIO=0
        JUCE_PLUGINHOST_AU=1
        JUCE_PLUGINHOST_LADSPA=0
    )
endif()

# --- Linux Definitions ---
if(PLATFORM_LINUX)
    list(APPEND COMMON_DEFINITIONS
        JUCE_ASIO=0
        JUCE_ALSA=1
        JUCE_JACK=1
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=1
    )
endif()

# --- VST2 Support - DISABLED ---
# VST2 hosting removed due to stability issues with unofficial SDKs
list(APPEND COMMON_DEFINITIONS JUCE_PLUGINHOST_VST=0)

# --- CLAP Hosting Support ---
if(CLAP_HOSTING_SUPPORT)
    list(APPEND COMMON_DEFINITIONS SUBTERRANEUM_CLAP_HOSTING=1)
else()
    list(APPEND COMMON_DEFINITIONS SUBTERRANEUM_CLAP_HOSTING=0)
endif()

target_compile_definitions(SubterraneumApp PUBLIC ${COMMON_DEFINITIONS})

# =============================================================================
# Include Directories
# =============================================================================

# ASIO SDK (Windows)
if(PLATFORM_WINDOWS AND EXISTS "${ASIO_SDK_PATH}/common")
    target_include_directories(SubterraneumApp PRIVATE ${ASIO_SDK_PATH}/common)
endif()

# CLAP SDK (if available)
if(CLAP_SDK_FOUND)
    target_include_directories(SubterraneumApp PRIVATE ${CLAP_SDK_PATH}/include)
endif()

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(SubterraneumApp PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# macOS Frameworks
if(PLATFORM_MAC)
    target_link_libraries(SubterraneumApp PRIVATE
        "-framework CoreAudio"
        "-framework CoreMIDI"
        "-framework AudioUnit"
        "-framework AudioToolbox"
        "-framework CoreServices"
        "-framework Carbon"
        "-framework Cocoa"
        "-framework CoreFoundation"
        "-framework IOKit"
    )
    
    set_target_properties(SubterraneumApp PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.subcore.colosseum"
        MACOSX_BUNDLE_BUNDLE_NAME "Colosseum"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
    
    # Copy Info.plist if it exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/Info.plist")
        set_target_properties(SubterraneumApp PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
        )
    endif()
endif()

# Linux Libraries
if(PLATFORM_LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED alsa)
    pkg_check_modules(FREETYPE2 REQUIRED freetype2)
    
    target_link_libraries(SubterraneumApp PRIVATE
        ${ALSA_LIBRARIES}
        ${FREETYPE2_LIBRARIES}
        pthread
        dl
    )
    
    # JACK (optional)
    pkg_check_modules(JACK jack)
    if(JACK_FOUND)
        target_link_libraries(SubterraneumApp PRIVATE ${JACK_LIBRARIES})
    endif()
endif()

# CLAP Wrapper (if available)
if(CLAP_HOSTING_SUPPORT)
    target_link_libraries(SubterraneumApp PRIVATE clap-wrapper)
endif()

# =============================================================================
# Installation
# =============================================================================
if(PLATFORM_WINDOWS)
    install(TARGETS SubterraneumApp RUNTIME DESTINATION bin)
elseif(PLATFORM_MAC)
    install(TARGETS SubterraneumApp BUNDLE DESTINATION .)
elseif(PLATFORM_LINUX)
    install(TARGETS SubterraneumApp RUNTIME DESTINATION bin)
endif()

# =============================================================================
# Build Summary
# =============================================================================
message(STATUS "")
message(STATUS "╔══════════════════════════════════════════════════════════════╗")
message(STATUS "║           SUBTERRANEUM BUILD CONFIGURATION                   ║")
message(STATUS "╠══════════════════════════════════════════════════════════════╣")
message(STATUS "║ Platform:        ${CMAKE_SYSTEM_NAME}")
message(STATUS "║ JUCE Path:       ${JUCE_PATH}")
message(STATUS "╠══════════════════════════════════════════════════════════════╣")
message(STATUS "║ PLUGIN HOSTING FORMATS:                                      ║")
message(STATUS "║   VST3:          ✓ Enabled (built-in)                        ║")
message(STATUS "║   VST2:          ✗ Disabled (stability)                      ║")
if(PLATFORM_MAC)
message(STATUS "║   AudioUnit:     ✓ Enabled (macOS)                           ║")
else()
message(STATUS "║   AudioUnit:     - N/A (macOS only)                          ║")
endif()
if(CLAP_HOSTING_SUPPORT)
message(STATUS "║   CLAP:          ✓ Enabled                                   ║")
else()
message(STATUS "║   CLAP:          ✗ Disabled (clap-wrapper not found)         ║")
endif()
if(PLATFORM_LINUX)
message(STATUS "║   LADSPA:        ✓ Enabled (Linux)                           ║")
endif()
message(STATUS "╠══════════════════════════════════════════════════════════════╣")
if(PLATFORM_WINDOWS)
message(STATUS "║ AUDIO DRIVERS:   ASIO, WASAPI                                ║")
elseif(PLATFORM_MAC)
message(STATUS "║ AUDIO DRIVERS:   CoreAudio                                   ║")
elseif(PLATFORM_LINUX)
message(STATUS "║ AUDIO DRIVERS:   ALSA, JACK                                  ║")
endif()
message(STATUS "╚══════════════════════════════════════════════════════════════╝")
message(STATUS "")