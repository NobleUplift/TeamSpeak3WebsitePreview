cmake_minimum_required(VERSION 3.21)

# ---------------------------------------------------------------------------
# Plugin metadata — single source of truth. PLUGIN_VERSION is a cache var so CI
# can override it from the git tag:  cmake -DPLUGIN_VERSION=1.2.3 ...
# ---------------------------------------------------------------------------
project(ts3websitepreview
    VERSION 3.0
    DESCRIPTION "TeamSpeak 3 Website Preview plugin"
    LANGUAGES C CXX)

set(PLUGIN_NAME        "Website Preview")
set(PLUGIN_TYPE        "Plugin")
set(PLUGIN_VERSION     "${PROJECT_VERSION}" CACHE STRING "Plugin version baked into the binary")
set(PLUGIN_AUTHOR      "NobleUplift (Patrick Seiter)")
set(PLUGIN_DESCRIPTION "Fetches the title (and optionally description) of URLs sent in chat. Works for URL-only messages and for URLs embedded in typed messages.")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
# /MD (/MDd in Debug) for every target incl. the static gumbo lib, matching Qt (CMP0091=NEW at 3.21).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# ---------------------------------------------------------------------------
# Platform -> TS3 binary suffix + file extension (unchanged from before).
# ---------------------------------------------------------------------------
if(APPLE)
    set(TS3_ARCH_SUFFIX "mac")
    set(TS3_LIB_EXT     ".dylib")
elseif(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(TS3_ARCH_SUFFIX "win64")
    else()
        set(TS3_ARCH_SUFFIX "win32")
    endif()
    set(TS3_LIB_EXT ".dll")
else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(TS3_ARCH_SUFFIX "linux_amd64")
    else()
        set(TS3_ARCH_SUFFIX "linux_x86")
    endif()
    set(TS3_LIB_EXT ".so")
endif()
message(STATUS "Building TS3 plugin for ${TS3_ARCH_SUFFIX}")

# ---------------------------------------------------------------------------
# Dependencies (all via git submodules — no per-platform package management):
#   - TS3 SDK headers (ts3client-pluginsdk)
#   - Qt5 Widgets + Network — HTTP (QNetworkAccessManager), settings dialog
#     (QDialog), persistence (QSettings). Provided by the running TS3 client
#     (ships Qt 5.15.2), so linked but not shipped.
#   - Gumbo (google/gumbo-parser) — the HTML5 parser behind src/webparse.
#     Built from source into a STATIC lib, so nothing is bundled.
# ---------------------------------------------------------------------------
set(TS3_SDK_INCLUDE "${CMAKE_SOURCE_DIR}/ts3client-pluginsdk/include")
if(NOT EXISTS "${TS3_SDK_INCLUDE}/ts3_functions.h")
    message(FATAL_ERROR "TS3 SDK submodule missing. Run: git submodule update --init")
endif()

find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

# ---- Gumbo HTML5 parser: compile the submodule sources into a static lib ----
set(GUMBO_DIR "${CMAKE_SOURCE_DIR}/third_party/gumbo-parser")
if(NOT EXISTS "${GUMBO_DIR}/src/gumbo.h")
    message(FATAL_ERROR "gumbo-parser submodule missing. Run: git submodule update --init")
endif()
file(GLOB GUMBO_SOURCES CONFIGURE_DEPENDS "${GUMBO_DIR}/src/*.c")
add_library(gumbo STATIC ${GUMBO_SOURCES})
target_include_directories(gumbo PUBLIC "${GUMBO_DIR}/src")
set_target_properties(gumbo PROPERTIES POSITION_INDEPENDENT_CODE ON)  # linked into a shared MODULE
if(MSVC)
    # Gumbo uses POSIX <strings.h>/strcasecmp; upstream ships an MSVC shim for it.
    target_include_directories(gumbo PRIVATE "${GUMBO_DIR}/visualc/include")
    target_compile_definitions(gumbo PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

configure_file("${CMAKE_SOURCE_DIR}/cmake/plugin_version.h.in"
               "${CMAKE_BINARY_DIR}/generated/plugin_version.h" @ONLY)

# ===========================================================================
#  The plugin (MODULE = a runtime-loaded plugin).
# ===========================================================================
add_library(ts3websitepreview MODULE
    src/core.c            # portable C string logic (compiled as C)
    src/plugin.cpp        # C++/Qt entry points
    src/config.cpp        # QDialog settings + QSettings
    src/webparse.cpp)     # Gumbo title/OGP extraction

set_target_properties(ts3websitepreview PROPERTIES
    OUTPUT_NAME "ts3websitepreview_${TS3_ARCH_SUFFIX}"   # -> ..._win64.dll / _linux_amd64.so / _mac.dylib
    PREFIX ""
    SUFFIX "${TS3_LIB_EXT}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"   # Windows .dll
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")  # Linux/macOS .so/.dylib

target_include_directories(ts3websitepreview PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${TS3_SDK_INCLUDE}"
    "${CMAKE_BINARY_DIR}/generated")

target_link_libraries(ts3websitepreview PRIVATE Qt5::Widgets Qt5::Network gumbo)

if(WIN32)
    target_compile_definitions(ts3websitepreview PRIVATE
        WIN32 _WINDOWS TS3WEBSITEPREVIEW_EXPORTS UNICODE _UNICODE)
    set_target_properties(ts3websitepreview PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/implib")
else()
    target_compile_definitions(ts3websitepreview PRIVATE TS3WEBSITEPREVIEW_EXPORTS)
endif()

# ===========================================================================
#  Packaging into the installable .ts3_plugin is handled by CI
#  (.github/workflows/deploy.yml): it collects the four binaries from
#  build/**/out/ and zips them with package.ini — no bundled dependency DLLs.
# ===========================================================================

# ===========================================================================
#  Optional tests:
#    - ts3websitepreview_tests           Unity C tests for the core string logic
#    - ts3websitepreview_webparse_tests  QtTest for the title/OGP parser
#      cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build && ctest --test-dir build
# ===========================================================================
option(BUILD_TESTS "Build the unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()

    add_executable(ts3websitepreview_tests
        test/main_test.c
        test/test_url.c
        test/test_message.c
        test/test_inline.c
        test/unity/unity.c
        src/core.c)
    target_include_directories(ts3websitepreview_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/test" "${CMAKE_SOURCE_DIR}/src")
    if(WIN32)
        target_compile_definitions(ts3websitepreview_tests PRIVATE WIN32 _CONSOLE)
    endif()
    add_test(NAME core COMMAND ts3websitepreview_tests)

    find_package(Qt5 REQUIRED COMPONENTS Test)
    add_executable(ts3websitepreview_webparse_tests
        test/test_webparse.cpp
        src/webparse.cpp)
    target_include_directories(ts3websitepreview_webparse_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(ts3websitepreview_webparse_tests PRIVATE Qt5::Test gumbo)
    add_test(NAME webparse COMMAND ts3websitepreview_webparse_tests)
endif()
