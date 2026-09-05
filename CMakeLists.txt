cmake_minimum_required(VERSION 3.16)
project(hl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

enable_language(RC)

add_executable(hl WIN32 src/main.cpp src/launcher.cpp)

set(HL_RES "${CMAKE_CURRENT_BINARY_DIR}/hl.res")
add_custom_command(
    OUTPUT "${HL_RES}"
    COMMAND "${CMAKE_RC_COMPILER}" /nologo /i "${CMAKE_CURRENT_SOURCE_DIR}/res" /fo "${HL_RES}" "${CMAKE_CURRENT_SOURCE_DIR}/res/hl.rc"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/hl.rc" "${CMAKE_CURRENT_SOURCE_DIR}/res/game.ico"
    COMMENT "Embedding Valve game.ico"
    VERBATIM
)
target_sources(hl PRIVATE "${HL_RES}")
set_source_files_properties("${HL_RES}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
target_link_libraries(hl PRIVATE user32)
target_compile_definitions(hl PRIVATE _CRT_SECURE_NO_WARNINGS)
if (MSVC)
    target_compile_options(hl PRIVATE /utf-8)
endif()
set_target_properties(hl PROPERTIES OUTPUT_NAME hl)
