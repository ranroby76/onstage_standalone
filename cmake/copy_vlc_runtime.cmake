# ===================================================================
# copy_vlc_runtime.cmake
#
# Standalone script to copy all VLC runtime files required by
# OnStage Karaoke (Windows build).
#
# This includes:
#   - libvlc.dll
#   - libvlccore.dll
#   - VLC plugins directory
#
# Usage:
#   add_custom_command(TARGET OnStage POST_BUILD
#       COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_vlc_runtime.cmake
#   )
# ===================================================================

message(STATUS "Copying VLC runtime...")

# -------------------------------------------------------------------
# Paths
# -------------------------------------------------------------------
set(VLC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vlc-3.0.21")
set(VLC_DLL_DIR "${VLC_ROOT}")

# The final output directory for OnStage.exe
set(OUTPUT_DIR "$<TARGET_FILE_DIR:OnStage>")

# -------------------------------------------------------------------
# Copy required DLLs
# -------------------------------------------------------------------
message(STATUS "Copying libvlc.dll to ${OUTPUT_DIR}")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VLC_DLL_DIR}/libvlc.dll"
        "${OUTPUT_DIR}"
)

message(STATUS "Copying libvlccore.dll to ${OUTPUT_DIR}")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VLC_DLL_DIR}/libvlccore.dll"
        "${OUTPUT_DIR}"
)

# -------------------------------------------------------------------
# Copy plugins directory
# -------------------------------------------------------------------
set(VLC_PLUGIN_SRC "${VLC_ROOT}/plugins")
set(VLC_PLUGIN_DST "${OUTPUT_DIR}/plugins")

message(STATUS "Copying VLC plugins folder to ${VLC_PLUGIN_DST}")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${VLC_PLUGIN_SRC}"
        "${VLC_PLUGIN_DST}"
)

message(STATUS "VLC runtime copy complete.")
