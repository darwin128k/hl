cmake_minimum_required(VERSION 3.16)
project(vellum_host LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lvgl/CMakeLists.txt")
    message(FATAL_ERROR "lvgl missing. Junction or clone into vellum/host/lvgl (archive: H:/Vellum/engine/lvgl).")
endif()
add_subdirectory(lvgl)

add_executable(hl WIN32 src/main.cpp src/steam.cpp src/shell.cpp src/lvgl_win.cpp src/module.cpp src/prefetch.cpp)
target_link_libraries(hl PRIVATE lvgl user32 gdi32 gdiplus ole32 advapi32 shell32 ws2_32)
target_compile_definitions(hl PRIVATE _CRT_SECURE_NO_WARNINGS)
if (MSVC)
    target_compile_options(hl PRIVATE /utf-8)
endif()
set_target_properties(hl PROPERTIES OUTPUT_NAME hl)
