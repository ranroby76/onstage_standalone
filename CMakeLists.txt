# **Change:** I added logic to detect the OS. * **Windows:** Uses your local folders (`D:/...`) and links `winmm`, `comdlg32`, etc. * **Linux:** Looks for system-installed libraries (`libvlc`, `alsa`, `x11`, `freetype`) instead of local folders. <!-- end list -->

cmake_minimum_required(VERSION 3.22)

project(OnStage VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

# ================================================================
# OS Detection & Paths
# ================================================================
if (WIN32)
    set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
    set(JUCE_DIR     "${PROJECT_ROOT}/JUCE")
    set(ASIO_DIR     "${PROJECT_ROOT}/asiosdk")
    set(VLC_DIR      "${PROJECT_ROOT}/vlc-3.0.21")
    set(ASSETS_DIR   "${PROJECT_ROOT}/assets")
    
    # Windows Definitions
    add_definitions(-DJUCE_WINDOWS=1)
elseif (UNIX AND NOT APPLE)
    set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
    set(JUCE_DIR     "${PROJECT_ROOT}/JUCE") # Keep JUCE local or system? Assuming local for now
    set(ASSETS_DIR   "${PROJECT_ROOT}/assets")
    
    # Linux Definitions
    add_definitions(-DJUCE_LINUX=1)
    
    # Find Linux System Packages
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(VLC REQUIRED libvlc)
    pkg_check_modules(ALSA REQUIRED alsa)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    
    # Linux usually needs these for JUCE
    link_libraries(dl pthread rt)
endif()

set(SRC_DIR "${PROJECT_ROOT}/src")

# ================================================================
# JUCE Setup
# ================================================================
add_subdirectory(${JUCE_DIR})

# ================================================================
# Include directories
# ================================================================
include_directories(
    ${PROJECT_ROOT}
    ${SRC_DIR}
    ${SRC_DIR}/engine
    ${SRC_DIR}/dsp
    ${SRC_DIR}/UI
    
    # JUCE Modules
    ${JUCE_DIR}/modules
    ${JUCE_DIR}/modules/juce_core
    ${JUCE_DIR}/modules/juce_events
    ${JUCE_DIR}/modules/juce_data_structures
    ${JUCE_DIR}/modules/juce_graphics
    ${JUCE_DIR}/modules/juce_gui_basics
    ${JUCE_DIR}/modules/juce_gui_extra
    ${JUCE_DIR}/modules/juce_audio_basics
    ${JUCE_DIR}/modules/juce_audio_devices
    ${JUCE_DIR}/modules/juce_audio_formats
    ${JUCE_DIR}/modules/juce_audio_utils
    ${JUCE_DIR}/modules/juce_audio_processors
    ${JUCE_DIR}/modules/juce_dsp
)

if (WIN32)
    include_directories(
        ${ASIO_DIR}/common
        ${ASIO_DIR}/host
        ${VLC_DIR}/sdk/include
    )
elseif (UNIX AND NOT APPLE)
    include_directories(
        ${VLC_INCLUDE_DIRS}
        /usr/include/freetype2
    )
endif()

# ================================================================
# Source files
# ================================================================
set(ONSTAGE_SOURCES
    # --- Core ---
    ${SRC_DIR}/App.cpp
    ${SRC_DIR}/App.h
    ${SRC_DIR}/AudioEngine.cpp
    ${SRC_DIR}/AudioEngine.h
    ${SRC_DIR}/Main.cpp
    ${SRC_DIR}/IOSettingsManager.cpp
    ${SRC_DIR}/IOSettingsManager.h
    ${SRC_DIR}/PresetManager.cpp
    ${SRC_DIR}/PresetManager.h
    ${SRC_DIR}/AppLogger.h
    ${SRC_DIR}/RegistrationManager.cpp
    ${SRC_DIR}/RegistrationManager.h
    
    # --- Engine ---
    ${SRC_DIR}/engine/VideoSurfaceComponent.cpp
    ${SRC_DIR}/engine/VideoSurfaceComponent.h
    ${SRC_DIR}/engine/VLCMediaPlayer.cpp
    ${SRC_DIR}/engine/VLCMediaPlayer.h
    
    # --- DSP ---
    ${SRC_DIR}/dsp/EQProcessor.cpp
    ${SRC_DIR}/dsp/EQProcessor.h
    ${SRC_DIR}/dsp/ExciterProcessor.cpp
    ${SRC_DIR}/dsp/ExciterProcessor.h
    ${SRC_DIR}/dsp/ReverbProcessor.cpp
    ${SRC_DIR}/dsp/ReverbProcessor.h
    ${SRC_DIR}/dsp/CompressorProcessor.h
    ${SRC_DIR}/dsp/DelayProcessor.h
    ${SRC_DIR}/dsp/DynamicEQProcessor.h
    ${SRC_DIR}/dsp/HarmonizerProcessor.h
    
    # --- UI ---
    ${SRC_DIR}/UI/MainComponent.cpp
    ${SRC_DIR}/UI/MainComponent.h
    ${SRC_DIR}/UI/HeaderBar.cpp
    ${SRC_DIR}/UI/HeaderBar.h
    ${SRC_DIR}/UI/RegistrationComponent.h
    ${SRC_DIR}/UI/IOPage.cpp
    ${SRC_DIR}/UI/IOPage.h
    ${SRC_DIR}/UI/VocalsPage.cpp
    ${SRC_DIR}/UI/VocalsPage.h
    ${SRC_DIR}/UI/MediaPage.cpp
    ${SRC_DIR}/UI/MediaPage.h
    ${SRC_DIR}/UI/PlaylistComponent.cpp
    ${SRC_DIR}/UI/PlaylistComponent.h
    ${SRC_DIR}/UI/TrackBannerComponent.cpp
    ${SRC_DIR}/UI/TrackBannerComponent.h
    ${SRC_DIR}/UI/PlaylistDataStructures.h
    ${SRC_DIR}/UI/DebugConsole.h
    ${SRC_DIR}/UI/ManualComponent.h 
    
    # Panels & Controls
    ${SRC_DIR}/UI/CompressorPanel.h
    ${SRC_DIR}/UI/DelayPanel.h
    ${SRC_DIR}/UI/DualHandleSlider.h
    ${SRC_DIR}/UI/DynamicEQPanel.h
    ${SRC_DIR}/UI/EffectToggleButton.h
    ${SRC_DIR}/UI/EQPanel.h
    ${SRC_DIR}/UI/ExciterPanel.h
    ${SRC_DIR}/UI/HarmonizerPanel.h
    ${SRC_DIR}/UI/ReverbPanel.h
    ${SRC_DIR}/UI/StyledSlider.h
    ${SRC_DIR}/UI/SignalLed.h
    ${SRC_DIR}/UI/LevelMeter.h
    ${SRC_DIR}/UI/MasterMeter.cpp
    ${SRC_DIR}/UI/MasterMeter.h
)

# ================================================================
# Binary Assets
# ================================================================
juce_add_binary_data(OnStageAssets
    SOURCES
        "${PROJECT_ROOT}/icon.ico"
        "${ASSETS_DIR}/logo.png"
        "${ASSETS_DIR}/On_stage_logo.png"
        "${ASSETS_DIR}/ir.wav"
        "${ASSETS_DIR}/license.mid"
)

# ================================================================
# Executable
# ================================================================
if (WIN32)
    add_executable(OnStage WIN32
        ${ONSTAGE_SOURCES}
        "${PROJECT_ROOT}/resources.rc"
        "${PROJECT_ROOT}/icon.ico"
    )
else()
    add_executable(OnStage
        ${ONSTAGE_SOURCES}
    )
endif()

# ================================================================
# Target Properties
# ================================================================
if (WIN32)
    set_target_properties(OnStage PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
        VS_DEBUGGER_WORKING_DIRECTORY    "$<TARGET_FILE_DIR:OnStage>"
    )
    
    target_compile_definitions(OnStage PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_ASIO=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CAMERA=0
        JUCE_USE_CURL=0
        _CRT_SECURE_NO_WARNINGS
    )
elseif (UNIX AND NOT APPLE)
    set_target_properties(OnStage PROPERTIES
        OUTPUT_NAME "OnStage"
    )
    
    target_compile_definitions(OnStage PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_ALSA=1 
        JUCE_WEB_BROWSER=0
        JUCE_USE_CAMERA=0
        JUCE_USE_CURL=0
        JUCE_USE_X11=1
    )
endif()

# ================================================================
# Linking
# ================================================================
target_link_libraries(OnStage PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    OnStageAssets
)

if (WIN32)
    set(VLC_LIB      "${VLC_DIR}/sdk/lib/libvlc.lib")
    set(VLC_CORE_LIB "${VLC_DIR}/sdk/lib/libvlccore.lib")
    
    target_link_libraries(OnStage PRIVATE
        ${VLC_LIB}
        ${VLC_CORE_LIB}
        winmm
        ws2_32
        gdi32
        ole32
        uuid
        comdlg32
    )
    
    # Windows Post-Build: Copy DLLs
    add_custom_command(TARGET OnStage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VLC_DIR}/libvlc.dll" "$<TARGET_FILE_DIR:OnStage>"
        COMMENT "Copying libvlc.dll..."
    )
    add_custom_command(TARGET OnStage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VLC_DIR}/libvlccore.dll" "$<TARGET_FILE_DIR:OnStage>"
        COMMENT "Copying libvlccore.dll..."
    )
    add_custom_command(TARGET OnStage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VLC_DIR}/plugins" "$<TARGET_FILE_DIR:OnStage>/plugins"
        COMMENT "Copying VLC plugins..."
    )

elseif (UNIX AND NOT APPLE)
    target_link_libraries(OnStage PRIVATE
        ${VLC_LIBRARIES}
        ${ALSA_LIBRARIES}
        ${FREETYPE_LIBRARIES}
        X11
        Xext
        Xinerama
        asound
    )
endif()

message(STATUS "OnStage build configuration complete.")