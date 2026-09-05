cmake_minimum_required(VERSION 3.16)
project(hl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(hl WIN32 src/main.cpp)
target_link_libraries(hl PRIVATE user32)
target_compile_definitions(hl PRIVATE _CRT_SECURE_NO_WARNINGS)
if (MSVC)
    target_compile_options(hl PRIVATE /utf-8)
endif()
set_target_properties(hl PROPERTIES OUTPUT_NAME hl)
