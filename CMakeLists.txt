cmake_minimum_required(VERSION 3.24)

project(MicrophoneActivityWidget LANGUAGES CXX RC)

add_executable(MicrophoneActivityWidget WIN32
    src/main.cpp
    src/headset_adapters.cpp
    src/app.rc
)

target_compile_features(MicrophoneActivityWidget PRIVATE cxx_std_20)
target_compile_definitions(MicrophoneActivityWidget PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)

if(MSVC)
    target_compile_options(MicrophoneActivityWidget PRIVATE
        /W4
        /permissive-
        /utf-8
    )
    set_property(TARGET MicrophoneActivityWidget PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
else()
    target_compile_options(MicrophoneActivityWidget PRIVATE -Wall -Wextra)
    target_link_options(MicrophoneActivityWidget PRIVATE -municode -static)
endif()

target_link_libraries(MicrophoneActivityWidget PRIVATE
    comdlg32
    comctl32
    gdiplus
    hid
    ole32
    oleaut32
    propsys
    runtimeobject
    setupapi
    shell32
    shlwapi
    uuid
)

install(TARGETS MicrophoneActivityWidget RUNTIME DESTINATION .)
