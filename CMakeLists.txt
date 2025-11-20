cmake_minimum_required(VERSION 3.27)
project(NppVim LANGUAGES CXX RC)

set(SOURCES
    src/CommandMode.cpp
    src/Marks.cpp
    src/Motion.cpp
    src/NormalMode.cpp
    src/NppVim.cpp
    src/TextObject.cpp
    src/Utils.cpp
    src/VisualMode.cpp
    plugin/NppVim.rc
)

add_library(NppVim SHARED ${SOURCES})

target_compile_definitions(NppVim PRIVATE UNICODE _UNICODE)
target_compile_features(NppVim PRIVATE cxx_std_17)
target_include_directories(NppVim PRIVATE include plugin)

target_link_libraries(NppVim PRIVATE
    kernel32 user32 gdi32 shell32 ole32 oleaut32 advapi32 shlwapi uuid
)

# Always build to current binary directory
set_target_properties(NppVim PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    OUTPUT_NAME "NppVim"
)

# Manual architecture detection based on binary directory path
if(CMAKE_BINARY_DIR MATCHES ".*[\\/]win32[\\/]?$")
    set(DIST_DIR "${CMAKE_SOURCE_DIR}/NppVim")
    message(STATUS "Detected win32 build, distribution: ${DIST_DIR}")
elseif(CMAKE_BINARY_DIR MATCHES ".*[\\/]x64[\\/]?$")
    set(DIST_DIR "${CMAKE_SOURCE_DIR}/NppVim.x64")
    message(STATUS "Detected x64 build, distribution: ${DIST_DIR}")
elseif(CMAKE_BINARY_DIR MATCHES ".*[\\/]arm64[\\/]?$")
    set(DIST_DIR "${CMAKE_SOURCE_DIR}/NppVim.arm64")
    message(STATUS "Detected arm64 build, distribution: ${DIST_DIR}")
else()
    # Fallback based on pointer size
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DIST_DIR "${CMAKE_SOURCE_DIR}/NppVim.x64")
        message(STATUS "Fallback to x64, distribution: ${DIST_DIR}")
    else()
        set(DIST_DIR "${CMAKE_SOURCE_DIR}/NppVim")
        message(STATUS "Fallback to win32, distribution: ${DIST_DIR}")
    endif()
endif()

# For Release builds, copy files to distribution directory
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET NppVim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:NppVim>" "${DIST_DIR}/NppVim.dll"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${DIST_DIR}/LICENSE"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/tutor.txt" "${DIST_DIR}/tutor.txt"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/Readme.md" "${DIST_DIR}/Readme.md"
        COMMENT "Copying files to ${DIST_DIR}"
    )
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Source dir: ${CMAKE_SOURCE_DIR}")
message(STATUS "Binary dir: ${CMAKE_BINARY_DIR}")
