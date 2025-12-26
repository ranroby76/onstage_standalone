cmake_minimum_required(VERSION 3.22)

project(OnStage VERSION 1.0.0 LANGUAGES CXX)

# =========================
# JUCE
# =========================
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
add_subdirectory(${JUCE_DIR} JUCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# VLC (Windows Only)
# =========================
if (WIN32)
    set(VLC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vlc-3.0.21")
    set(VLC_INCLUDE_DIR "${VLC_ROOT}/sdk/include")
    set(VLC_LIB_DIR "${VLC_ROOT}/sdk/lib")
    set(VLC_RUNTIME_DIR "${VLC_ROOT}")
endif()

# =========================
# ASIO SDK (Windows Only)
# =========================
if (WIN32)
    set(ASIO_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asiosdk")
endif()

# =========================
# APPLICATION
# =========================
juce_add_gui_app(OnStage
    PRODUCT_NAME "OnStage"
    COMPANY_NAME "Subcore"
    VERSION "1.0.0"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/logo_256.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/logo_48.png"
)

# =========================
# SOURCES
# =========================

# Platform-specific media player
if (WIN32)
    set(MEDIA_PLAYER_SOURCES
        src/engine/VLCMediaPlayer.cpp
        src/engine/VLCMediaPlayer.h
        src/engine/VLCMediaPlayer_Desktop.cpp
        src/engine/VLCMediaPlayer_Desktop.h
    )
elseif(APPLE)
    set(MEDIA_PLAYER_SOURCES
        src/engine/AVFMediaPlayer.h
        src/engine/AVFMediaPlayer_Mac.h
        src/engine/AVFMediaPlayer_Mac.mm
    )
endif()

target_sources(OnStage PRIVATE
    ${MEDIA_PLAYER_SOURCES}

    src/App.cpp
    src/App.h
    src/AppLogger.h
    src/AudioEngine.cpp
    src/AudioEngine.h

    src/engine/VideoSurfaceComponent.cpp
    src/engine/VideoSurfaceComponent.h

    src/IPC/SharedMemoryManager.cpp
    src/IPC/SharedMemoryManager.h

    src/IOSettingsManager.cpp
    src/IOSettingsManager.h
    src/PresetManager.cpp
    src/PresetManager.h
    src/RegistrationManager.cpp
    src/RegistrationManager.h

    src/UI/MainComponent.cpp
    src/UI/MainComponent.h
    src/UI/HeaderBar.cpp
    src/UI/HeaderBar.h
    src/UI/MediaPage.cpp
    src/UI/MediaPage.h
    src/UI/VocalsPage.cpp
    src/UI/VocalsPage.h
    src/UI/IOPage.cpp
    src/UI/IOPage.h
    src/UI/PlaylistComponent.cpp
    src/UI/PlaylistComponent.h
    src/UI/TrackBannerComponent.cpp
    src/UI/TrackBannerComponent.h
    src/UI/RegistrationComponent.h
    src/UI/ManualComponent.h

    src/UI/EQPanel.h
    src/UI/CompressorPanel.h
    src/UI/ExciterPanel.h
    src/UI/SculptPanel.h
    src/UI/DelayPanel.h
    src/UI/ReverbPanel.h
    src/UI/HarmonizerPanel.h
    src/UI/DynamicEQPanel.h

    src/UI/StyledSlider.h
    src/UI/DualHandleSlider.h
    src/UI/EffectToggleButton.h
    src/UI/SignalLed.h
    src/UI/LevelMeter.h
    src/UI/MasterMeter.cpp
    src/UI/MasterMeter.h
    src/UI/DebugConsole.h
    src/UI/LongPressDetector.h

    src/dsp/EQProcessor.cpp
    src/dsp/EQProcessor.h
    src/dsp/CompressorProcessor.h
    src/dsp/ExciterProcessor.cpp
    src/dsp/ExciterProcessor.h
    src/dsp/SculptProcessor.cpp
    src/dsp/SculptProcessor.h
    src/dsp/DelayProcessor.h
    src/dsp/ReverbProcessor.cpp
    src/dsp/ReverbProcessor.h
    src/dsp/HarmonizerProcessor.h
    src/dsp/DynamicEQProcessor.h
)

# =========================
# BINARY DATA (FIXED)
# =========================
file(GLOB_RECURSE ONSTAGE_ASSETS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*"
)

juce_add_binary_data(OnStageAssets
    SOURCES
        ${ONSTAGE_ASSETS}
)

target_link_libraries(OnStage PRIVATE OnStageAssets)

# =========================
# INCLUDES
# =========================
if (WIN32)
    target_include_directories(OnStage PRIVATE
        ${VLC_INCLUDE_DIR}
        ${ASIO_SDK_DIR}/common
    )
endif()

# =========================
# LINKING
# =========================
if (WIN32)
    target_link_directories(OnStage PRIVATE
        ${VLC_LIB_DIR}
    )
    
    target_link_libraries(OnStage PRIVATE
        libvlc
        libvlccore
    )
endif()

target_link_libraries(OnStage PRIVATE
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
)

# Mac-specific frameworks
if (APPLE)
    target_link_libraries(OnStage PRIVATE
        "-framework AVFoundation"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework CoreAudio"
        "-framework Accelerate"
    )
endif()

# =========================
# DEFINES
# =========================
target_compile_definitions(OnStage PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:OnStage,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:OnStage,JUCE_VERSION>"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

if (WIN32)
    target_compile_definitions(OnStage PRIVATE
        JUCE_ASIO=1
    )
endif()

# =========================
# POST-BUILD: VLC RUNTIME (Windows only)
# =========================
if (WIN32)
    add_custom_command(TARGET OnStage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VLC_RUNTIME_DIR}/libvlc.dll"
            "$<TARGET_FILE_DIR:OnStage>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VLC_RUNTIME_DIR}/libvlccore.dll"
            "$<TARGET_FILE_DIR:OnStage>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VLC_RUNTIME_DIR}/plugins"
            "$<TARGET_FILE_DIR:OnStage>/plugins"
    )
endif()