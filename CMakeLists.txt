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

target_include_directories(NppVim PRIVATE include plugin)

target_compile_features(NppVim PRIVATE cxx_std_17)

target_link_libraries(NppVim PRIVATE
    kernel32 user32 gdi32 shell32 ole32 oleaut32 advapi32 shlwapi uuid
)
