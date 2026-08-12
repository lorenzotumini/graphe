cmake_minimum_required(VERSION 3.20)

project(graphe LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(GRAPHE_FETCH_RAYLIB "Fetch raylib with CMake when no system package is found" ON)

set(GRAPHE_RAYLIB_FETCHED OFF)
find_package(raylib QUIET)

if(NOT raylib_FOUND)
    if(NOT GRAPHE_FETCH_RAYLIB)
        message(FATAL_ERROR "raylib was not found. Install raylib or configure with -DGRAPHE_FETCH_RAYLIB=ON.")
    endif()

    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
    set(SUPPORT_SCREEN_CAPTURE OFF CACHE BOOL "" FORCE)
    set(SUPPORT_CLIPBOARD_IMAGE OFF CACHE BOOL "" FORCE)
    set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(raylib)
    set(GRAPHE_RAYLIB_FETCHED ON)
endif()

include(FetchContent)
FetchContent_Declare(
    raygui
    URL https://github.com/raysan5/raygui/archive/refs/tags/4.0.tar.gz
    URL_HASH SHA256=299c8fcabda68309a60dc858741b76c32d7d0fc533cdc2539a55988cee236812
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(raygui)

add_executable(graphe
    src/graph.c
    src/graph_io.c
    src/layout.c
    src/main.c
    src/platform_window.c
    src/raygui_impl.c
    src/render.c
    src/traversal.c
)

target_include_directories(graphe PRIVATE src ${raygui_SOURCE_DIR}/src)
target_link_libraries(graphe PRIVATE raylib)

include(CTest)
if(BUILD_TESTING)
    add_executable(graphe_tests
        tests/graph_traversal_tests.c
        src/graph.c
        src/graph_io.c
        src/layout.c
        src/traversal.c
    )
    target_include_directories(graphe_tests PRIVATE src)
    if(NOT WIN32)
        target_link_libraries(graphe_tests PRIVATE m)
    endif()
    add_test(NAME graphe_tests COMMAND graphe_tests)
    set_tests_properties(graphe_tests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

if(WIN32)
    target_compile_definitions(graphe PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(BUILD_TESTING)
        target_compile_definitions(graphe_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_link_libraries(graphe PRIVATE dwmapi)
endif()

if(MSVC)
    set_source_files_properties(src/raygui_impl.c PROPERTIES COMPILE_OPTIONS /w)
    target_compile_options(graphe PRIVATE /W4)
else()
    set_source_files_properties(src/raygui_impl.c PROPERTIES COMPILE_OPTIONS -w)
    if(GRAPHE_RAYLIB_FETCHED)
        # Keep diagnostics strict for Graphe while treating fetched raylib as the
        # third-party dependency it is. GCC otherwise reports warnings from
        # raylib's generated configuration definitions.
        target_compile_options(raylib PRIVATE -w)
    endif()
    target_compile_options(graphe PRIVATE -Wall -Wextra -pedantic)
endif()
