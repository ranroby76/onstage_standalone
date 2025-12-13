cmake_minimum_required(VERSION 3.22)

set(JUCE_SOURCE_DIR "D:/Workspace/onstage_with_chatgpt_6/JUCE")
set(JUCE_INSTALL_DIR "D:/Workspace/onstage_with_chatgpt_6/JUCE/install")

execute_process(
    COMMAND ${CMAKE_COMMAND} -B ${JUCE_SOURCE_DIR}/build -S ${JUCE_SOURCE_DIR} -DCMAKE_INSTALL_PREFIX=${JUCE_INSTALL_DIR}
)

execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${JUCE_SOURCE_DIR}/build --target install --config Release
)
