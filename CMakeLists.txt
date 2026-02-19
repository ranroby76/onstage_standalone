# D:\Workspace\ONSTAGE_WIRED\CMakeLists.txt

cmake_minimum_required(VERSION 3.22)

project(OnStage VERSION 1.0.0 LANGUAGES CXX C OBJCXX)

# =========================
# JUCE
# =========================
# Allow CI to override JUCE location via -DJUCE_DIR=...
if (NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
endif()
add_subdirectory(${JUCE_DIR} JUCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# VLC (Windows Only)
# =========================
if (WIN32)
    if (NOT DEFINED VLC_ROOT)
        set(VLC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../vlc-3.0.21")
    endif()
    set(VLC_INCLUDE_DIR "${VLC_ROOT}/sdk/include")
    set(VLC_LIB_DIR "${VLC_ROOT}/sdk/lib")
    set(VLC_RUNTIME_DIR "${VLC_ROOT}")
endif()

# =========================
# RUBBERBAND (single-file build, no external build step needed)
# =========================
set(RUBBERBAND_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/rubberband4")
set(RUBBERBAND_INCLUDE_DIR "${RUBBERBAND_ROOT}")

# =========================
# APPLICATION
# =========================
juce_add_gui_app(OnStage
    PRODUCT_NAME "OnStage"
    COMPANY_NAME "Fanan"
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
else()
    set(MEDIA_PLAYER_SOURCES
        src/engine/NullMediaPlayer.h
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

    # ==============================
    # NEW — Graph layer (Phase 1)
    # ==============================
    src/graph/EffectNodes.h
    src/graph/EffectNodes.cpp
    src/graph/PlaybackNode.h
    src/graph/PlaybackNode.cpp
    src/graph/OnStageGraph.h
    src/graph/OnStageGraph.cpp
    src/graph/GraphSerializer.h
    src/graph/GraphSerializer.cpp

    # ==============================
    # UI — Pages & Components
    # ==============================
    src/UI/MainComponent.cpp
    src/UI/MainComponent.h
    src/UI/HeaderBar.cpp
    src/UI/HeaderBar.h
    src/UI/MediaPage.cpp
    src/UI/MediaPage.h
    src/UI/IOPage.cpp
    src/UI/IOPage.h
    src/UI/PlaylistComponent.cpp
    src/UI/PlaylistComponent.h
    src/UI/TrackBannerComponent.cpp
    src/UI/TrackBannerComponent.h
    src/UI/RegistrationComponent.h
    src/UI/ManualComponent.h

    # ==============================
    # NEW — Internal Plugin Browser
    # ==============================
    src/UI/InternalPluginBrowser.h
    src/UI/WorkspaceManager.h

    # ==============================
    # NEW — Wiring Canvas (Phase 2)
    # ==============================
    src/UI/WiringStyle.h
    src/UI/WiringCanvas.h
    src/UI/WiringCanvas_Core.cpp
    src/UI/WiringCanvas_Paint.cpp
    src/UI/WiringCanvas_Layout.cpp
    src/UI/WiringCanvas_Mouse.cpp
    src/UI/WiringCanvas_Connections.cpp
    src/UI/WiringCanvas_Menu.cpp
    src/UI/WiringCanvas_NodeWindows.cpp

    # ==============================
    # UI — Effect Panels
    # ==============================
    src/UI/EQPanel.h
    src/UI/CompressorPanel.h
    src/UI/GatePanel.h
    src/UI/ExciterPanel.h
    src/UI/SculptPanel.h
    src/UI/DelayPanel.h
    src/UI/ReverbPanel.h
    src/UI/StudioReverbPanel.h
    src/UI/HarmonizerPanel.h
    src/UI/DynamicEQPanel.h
    src/UI/PreAmpPanel.h
    src/UI/DeEsserPanel.h
    src/UI/SaturationPanel.h
    src/UI/DoublerPanel.h
    src/UI/MasterPanel.h
    src/UI/TransientSplitterPanel.h

    # ==============================
    # UI — Widgets (existing)
    # ==============================
    src/UI/StyledSlider.h
    src/UI/DualHandleSlider.h
    src/UI/EffectToggleButton.h
    src/UI/SignalLed.h
    src/UI/LevelMeter.h
    src/UI/MasterMeter.cpp
    src/UI/MasterMeter.h
    src/UI/DebugConsole.h
    src/UI/LongPressDetector.h

    # ==============================
    # DSP Processors
    # ==============================
    src/dsp/EQProcessor.cpp
    src/dsp/EQProcessor.h
    src/dsp/CompressorProcessor.h
    src/dsp/GateProcessor.h
    src/dsp/ExciterProcessor.cpp
    src/dsp/ExciterProcessor.h
    src/dsp/SculptProcessor.cpp
    src/dsp/SculptProcessor.h
    src/dsp/DelayProcessor.h
    src/dsp/DelayDSP_Oxide.h
    src/dsp/DelayDSP_Warp.h
    src/dsp/DelayDSP_Crystal.h
    src/dsp/DelayDSP_Drift.h
    src/dsp/ReverbProcessor.h
    src/dsp/StudioReverbProcessor.h
    src/dsp/RoomReverbProcessor.h
    src/dsp/ChamberReverbProcessor.h
    src/dsp/SpaceReverbProcessor.h
    src/dsp/PlateReverbProcessor.h
    src/dsp/HarmonizerProcessor.h
    src/dsp/RubberBandPitchShifter.h
    src/dsp/DynamicEQProcessor.h
    src/dsp/PreAmpProcessor.h
    src/dsp/DeEsserProcessor.h
    src/dsp/SaturationProcessor.h
    src/dsp/DoublerProcessor.h
    src/dsp/MasterProcessor.h
    src/dsp/RecorderProcessor.cpp    # NEW
    src/dsp/RecorderProcessor.h      # NEW
    # src/dsp/TunerProcessor.h              # DISABLED
    src/dsp/TransientSplitterProcessor.cpp
    src/dsp/TransientSplitterProcessor.h

    # ==============================
    # RubberBand (single-file build)
    # ==============================
    ${RUBBERBAND_ROOT}/single/RubberBandSingle.cpp

    # ==============================
    # Guitar — DSP Processors
    # ==============================
    src/guitar/OverdriveProcessor.h
    src/guitar/DistortionProcessor.h
    src/guitar/FuzzProcessor.h
    src/guitar/GuitarChorusProcessor.h
    src/guitar/GuitarFlangerProcessor.h
    src/guitar/GuitarPhaserProcessor.h
    src/guitar/GuitarTremoloProcessor.h
    src/guitar/GuitarVibratoProcessor.h
    src/guitar/GuitarToneProcessor.h
    src/guitar/GuitarRotaryProcessor.h
    src/guitar/GuitarWahProcessor.h
    src/guitar/GuitarReverbProcessor.h
    src/guitar/GuitarNoiseGateProcessor.h
    src/guitar/ToneStackProcessor.h
    src/guitar/CabSimProcessor.h
    src/guitar/CabIRProcessor.h
    src/guitar/CabIRPanel.h
    src/guitar/CabIRNode.h

    # Guitar — Node Wrappers & UI Panels
    src/guitar/GuitarNodes.h
    src/guitar/GuitarPanels.h
)

# Fix MSVC min/max macro conflict with RubberBand's std::numeric_limits usage
if (MSVC)
    set_source_files_properties(
        ${RUBBERBAND_ROOT}/single/RubberBandSingle.cpp
        PROPERTIES COMPILE_FLAGS "/DNOMINMAX"
    )
endif()

# NOTE: VocalsPage.cpp / VocalsPage.h removed from build.
#       Routing is now handled by WiringCanvas + OnStageGraph.

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
target_include_directories(OnStage PRIVATE
    ${RUBBERBAND_INCLUDE_DIR}
)

if (WIN32)
    target_include_directories(OnStage PRIVATE
        ${VLC_INCLUDE_DIR}
    )
endif()

# =========================
# LINKING
# =========================

# RubberBand is compiled directly via single-file build (no separate library)

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
    juce::juce_audio_utils
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
            "${VLC_RUNTIME_DIR}/libvlccore.dll"
            "$<TARGET_FILE_DIR:OnStage>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VLC_RUNTIME_DIR}/plugins"
            "$<TARGET_FILE_DIR:OnStage>/plugins"
        COMMENT "Copying VLC runtime files..."
    )
endif()

# =========================
# OUTPUT DIRECTORIES
# =========================
set_target_properties(OnStage PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/release"
)
