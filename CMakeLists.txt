# #D:\Workspace\onstage_colosseum_upgrade\CMakeLists.txt
# =============================================================================
# OnStage - Mini Karaoke DAW / Live Singing Plugin Host
# CMakeLists.txt
# VLC-powered Media Tab (video/audio playback) - Windows & macOS
# =============================================================================

cmake_minimum_required(VERSION 3.22)
project(OnStage VERSION 1.0.0 LANGUAGES CXX C OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Platform Detection
# =============================================================================
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(PLATFORM_MAC TRUE)
else()
    set(PLATFORM_LINUX TRUE)
endif()

# =============================================================================
# JUCE Setup (external — not inside project tree)
# =============================================================================
set(JUCE_PATH "D:/Workspace/JUCE" CACHE PATH "Path to JUCE framework")
add_subdirectory(${JUCE_PATH} JUCE)

# =============================================================================
# VLC SDK Setup
# - Windows: vlc-3.0.21/ in project tree
# - macOS CI: passed via -DVLC_ROOT=...
# - macOS local: vlc-macos/ in project tree (fallback)
# =============================================================================
if(PLATFORM_WINDOWS)
    set(VLC_ROOT "${CMAKE_SOURCE_DIR}/vlc-3.0.21" CACHE PATH "Path to VLC SDK")
    set(VLC_INCLUDE_DIR "${VLC_ROOT}/sdk/include")
    set(VLC_LIB_DIR "${VLC_ROOT}/sdk/lib")
    set(VLC_RUNTIME_DIR "${VLC_ROOT}")
    set(VLC_PLUGINS_DIR "${VLC_ROOT}/plugins")
elseif(PLATFORM_MAC)
    # VLC_ROOT can be passed from CI via -DVLC_ROOT=...
    if(NOT DEFINED VLC_ROOT OR VLC_ROOT STREQUAL "")
        set(VLC_ROOT "${CMAKE_SOURCE_DIR}/vlc-macos" CACHE PATH "Path to VLC SDK (macOS)")
    endif()
    set(VLC_INCLUDE_DIR "${VLC_ROOT}/include")
    set(VLC_LIB_DIR "${VLC_ROOT}/lib")
    set(VLC_RUNTIME_DIR "${VLC_ROOT}/lib")
    set(VLC_PLUGINS_DIR "${VLC_ROOT}/plugins")
    
    message(STATUS "macOS VLC SDK: ${VLC_ROOT}")
endif()

# =============================================================================
# macOS Icon
# =============================================================================
if(PLATFORM_MAC)
    set(MACOS_ICON_FILE "${CMAKE_SOURCE_DIR}/assets/Icon.icns")
endif()

# =============================================================================
# Standalone Application Target
# =============================================================================
juce_add_gui_app(OnStageApp
    PRODUCT_NAME "OnStage"
    COMPANY_NAME "Fanan"
    BUNDLE_ID "com.fanan.onstage"
    # macOS specific settings
    ICON_BIG "${MACOS_ICON_FILE}"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "OnStage needs access to your microphone for live vocal input."
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS "com.apple.security.cs.allow-jit;com.apple.security.cs.allow-unsigned-executable-memory;com.apple.security.cs.disable-library-validation;com.apple.security.device.audio-input"
    PLIST_TO_MERGE "${CMAKE_SOURCE_DIR}/src/Info.plist"
    APP_SANDBOX_ENABLED FALSE
)

juce_generate_juce_header(OnStageApp)

# =============================================================================
# Platform-Specific Media Player Sources
# VLC is used on both Windows and macOS
# =============================================================================
if(PLATFORM_WINDOWS OR PLATFORM_MAC)
    set(MEDIA_PLAYER_SOURCES
        src/VLCMediaPlayer.cpp
        src/VLCMediaPlayer_Desktop.cpp
    )
else()
    set(MEDIA_PLAYER_SOURCES)
endif()

# =============================================================================
# Source Files
# =============================================================================
target_sources(OnStageApp PRIVATE
    ${MEDIA_PLAYER_SOURCES}

    src/VideoSurfaceComponent.cpp
    src/PlaybackNode.cpp
    src/MediaPage.cpp
    src/PlaylistComponent.cpp
    src/TrackBannerComponent.cpp

    src/PluginProcessor_01_Formats.cpp
    src/PluginProcessor_02_CtorDtor.cpp
    src/PluginProcessor_03_SettingsAndListener.cpp
    src/PluginProcessor_04_IOChannels.cpp
    src/PluginProcessor_05_Graph.cpp
    src/PluginProcessor_06_AudioAndPresets.cpp

    src/PluginManagerTab_Main.cpp
    src/PluginManagerTab_Scanner.cpp
    src/PluginManagerTab_AutoScanner.cpp
    src/PluginManagerTab_ScanOptions.cpp
    src/PluginManagerTab_Folders.cpp

    src/OutOfProcessScanner.cpp
    src/PluginScanWorker.cpp
    src/VST3ModuleScanner.cpp
    src/BackgroundPluginScanner.cpp

    src/GraphCanvas_Core.cpp
    src/GraphCanvas_Paint.cpp
    src/GraphCanvas_Layout.cpp
    src/GraphCanvas_Connections.cpp
    src/GraphCanvas_Mouse.cpp
    src/GraphCanvas_Tooltips.cpp
    src/GraphCanvas_PluginMenu.cpp
    src/GraphCanvas_PluginWindow.cpp
    src/GraphCanvas_DragDrop.cpp

    src/PluginEditor.cpp
    src/UIComponents.cpp
    src/MidiSelectors.cpp
    src/MixerStrip.cpp
    src/MixerView.cpp
    src/InstrumentSelector.cpp

    src/PluginBrowserPanel.cpp

    src/AudioSettingsTab.cpp
    src/StudioTab.cpp
    src/ManualTab.cpp
    src/RegistrationTab.cpp

    src/MeteringProcessor.cpp
    src/SimpleConnectorProcessor.cpp
    src/StereoMeterProcessor.cpp
    src/RecorderProcessor.cpp
    src/TransientSplitterProcessor.cpp
    src/ContainerProcessor.cpp

    src/StandaloneMain.cpp
    src/RegistrationManager.cpp
)

# =============================================================================
# Windows Icon via Resource File
# =============================================================================
if(PLATFORM_WINDOWS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/resources.rc")
        target_sources(OnStageApp PRIVATE "${CMAKE_SOURCE_DIR}/src/resources.rc")
        message(STATUS "Using resource file: src/resources.rc")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/resources.rc")
        target_sources(OnStageApp PRIVATE "${CMAKE_SOURCE_DIR}/resources.rc")
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
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_PLUGINHOST_VST3=1
)

if(PLATFORM_WINDOWS)
    list(APPEND COMMON_DEFINITIONS
        JUCE_WIN_PER_MONITOR_DPI_AWARE=1
        _CRT_SECURE_NO_WARNINGS
    )
endif()

if(PLATFORM_MAC)
    list(APPEND COMMON_DEFINITIONS
        JUCE_PLUGINHOST_AU=1
    )
endif()

if(PLATFORM_LINUX)
    list(APPEND COMMON_DEFINITIONS
        JUCE_PLUGINHOST_LADSPA=1
    )
endif()

# =============================================================================
# VST2 Support - Manual loading (Windows local builds only)
# =============================================================================
set(VST2_SDK_PATH "D:/Workspace/vst2sdk" CACHE PATH "Path to VST2 SDK")

if(EXISTS "${VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffectx.h")
    message(STATUS "VST2 SDK found - enabling VST2 hosting support")
    target_include_directories(OnStageApp PRIVATE ${VST2_SDK_PATH})
    target_compile_definitions(OnStageApp PRIVATE JUCE_PLUGINHOST_VST=1)
else()
    message(STATUS "VST2 SDK not found - VST2 hosting disabled")
    target_compile_definitions(OnStageApp PRIVATE JUCE_PLUGINHOST_VST=0)
endif()

target_compile_definitions(OnStageApp PRIVATE ${COMMON_DEFINITIONS})

# =============================================================================
# JUCE Module Setup
# =============================================================================
target_link_libraries(OnStageApp PRIVATE
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
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# =============================================================================
# Include Paths
# =============================================================================
target_include_directories(OnStageApp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# =============================================================================
# VLC SDK Include & Link (Windows)
# =============================================================================
if(PLATFORM_WINDOWS)
    target_include_directories(OnStageApp PRIVATE ${VLC_INCLUDE_DIR})
    target_link_directories(OnStageApp PRIVATE ${VLC_LIB_DIR})
    target_link_libraries(OnStageApp PRIVATE libvlc libvlccore)
endif()

# =============================================================================
# VLC SDK Include & Link (macOS)
# =============================================================================
if(PLATFORM_MAC)
    target_include_directories(OnStageApp PRIVATE ${VLC_INCLUDE_DIR})
    target_link_directories(OnStageApp PRIVATE ${VLC_LIB_DIR})
    target_link_libraries(OnStageApp PRIVATE vlc vlccore)
endif()

# =============================================================================
# ASIO Support (Windows)
# =============================================================================
set(ASIO_SDK_PATH "D:/Workspace/ASIOSDK" CACHE PATH "Path to ASIO SDK")

if(PLATFORM_WINDOWS AND EXISTS "${ASIO_SDK_PATH}/common/asio.h")
    message(STATUS "ASIO SDK found - enabling ASIO support")
    target_compile_definitions(OnStageApp PRIVATE JUCE_ASIO=1)
    target_include_directories(OnStageApp PRIVATE
        ${ASIO_SDK_PATH}/common
        ${ASIO_SDK_PATH}/host
    )
else()
    message(STATUS "ASIO SDK not found - building without ASIO support")
endif()

# =============================================================================
# CLAP Support
# =============================================================================
if(TARGET clap-wrapper)
    target_link_libraries(OnStageApp PRIVATE clap-wrapper)
    message(STATUS "CLAP wrapper enabled")
endif()

# =============================================================================
# Post-Build: VLC Runtime DLLs + plugins (Windows)
# =============================================================================
if(PLATFORM_WINDOWS)
    add_custom_command(TARGET OnStageApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VLC_RUNTIME_DIR}/libvlc.dll"
            "${VLC_RUNTIME_DIR}/libvlccore.dll"
            "$<TARGET_FILE_DIR:OnStageApp>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VLC_PLUGINS_DIR}"
            "$<TARGET_FILE_DIR:OnStageApp>/plugins"
        COMMENT "Copying VLC runtime files (Windows)..."
    )
endif()

# =============================================================================
# Post-Build: VLC dylibs + plugins (macOS)
# Copies to Contents/Frameworks inside the app bundle
# =============================================================================
if(PLATFORM_MAC)
    add_custom_command(TARGET OnStageApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:OnStageApp>/Contents/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VLC_LIB_DIR}"
            "$<TARGET_BUNDLE_DIR:OnStageApp>/Contents/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VLC_PLUGINS_DIR}"
            "$<TARGET_BUNDLE_DIR:OnStageApp>/Contents/Frameworks/plugins"
        COMMENT "Copying VLC runtime files to app bundle (macOS)..."
    )
endif()

# =============================================================================
# macOS Post-Build: Copy icon to bundle
# =============================================================================
if(PLATFORM_MAC AND MACOS_ICON_FILE)
    add_custom_command(TARGET OnStageApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MACOS_ICON_FILE}"
            "$<TARGET_BUNDLE_DIR:OnStageApp>/Contents/Resources/Icon.icns"
        COMMENT "Copying macOS icon to bundle"
    )
endif()
