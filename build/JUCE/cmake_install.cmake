# Install script for directory: D:/Workspace/JUCE

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files/Colosseum")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Workspace/Subterraneum_plugins_daw/build/JUCE/modules/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Workspace/Subterraneum_plugins_daw/build/JUCE/extras/Build/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/JUCE-8.0.11" TYPE FILE FILES
    "D:/Workspace/Subterraneum_plugins_daw/build/JUCE/JUCEConfigVersion.cmake"
    "D:/Workspace/Subterraneum_plugins_daw/build/JUCE/JUCEConfig.cmake"
    "D:/Workspace/JUCE/extras/Build/CMake/JUCECheckAtomic.cmake"
    "D:/Workspace/JUCE/extras/Build/CMake/JUCEHelperTargets.cmake"
    "D:/Workspace/JUCE/extras/Build/CMake/JUCEModuleSupport.cmake"
    "D:/Workspace/JUCE/extras/Build/CMake/JUCEUtils.cmake"
    "D:/Workspace/JUCE/extras/Build/CMake/JuceLV2Defines.h.in"
    "D:/Workspace/JUCE/extras/Build/CMake/LaunchScreen.storyboard"
    "D:/Workspace/JUCE/extras/Build/CMake/PIPAudioProcessor.cpp.in"
    "D:/Workspace/JUCE/extras/Build/CMake/PIPAudioProcessorWithARA.cpp.in"
    "D:/Workspace/JUCE/extras/Build/CMake/PIPComponent.cpp.in"
    "D:/Workspace/JUCE/extras/Build/CMake/PIPConsole.cpp.in"
    "D:/Workspace/JUCE/extras/Build/CMake/RecentFilesMenuTemplate.nib"
    "D:/Workspace/JUCE/extras/Build/CMake/UnityPluginGUIScript.cs.in"
    "D:/Workspace/JUCE/extras/Build/CMake/checkBundleSigning.cmake"
    "D:/Workspace/JUCE/extras/Build/CMake/copyDir.cmake"
    "D:/Workspace/JUCE/extras/Build/CMake/juce_runtime_arch_detection.cpp"
    "D:/Workspace/JUCE/extras/Build/CMake/juce_LinuxSubprocessHelper.cpp"
    )
endif()

