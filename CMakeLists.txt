cmake_minimum_required(VERSION 3.24)

project(MuteAlert LANGUAGES CXX RC)

add_executable(MuteAlert WIN32
    src/main.cpp
    src/headset_adapters.cpp
    src/app.rc
)

target_compile_features(MuteAlert PRIVATE cxx_std_20)
target_compile_definitions(MuteAlert PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)

if(MSVC)
    target_compile_options(MuteAlert PRIVATE
        /W4
        /permissive-
        /utf-8
    )
    set_property(TARGET MuteAlert PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
else()
    target_compile_options(MuteAlert PRIVATE -Wall -Wextra)
    target_link_options(MuteAlert PRIVATE -municode -static)
endif()

target_link_libraries(MuteAlert PRIVATE
    advapi32
    comdlg32
    comctl32
    gdi32
    gdiplus
    hid
    ole32
    oleaut32
    propsys
    runtimeobject
    setupapi
    shell32
    shlwapi
    user32
    uuid
)

install(TARGETS MuteAlert RUNTIME DESTINATION .)
