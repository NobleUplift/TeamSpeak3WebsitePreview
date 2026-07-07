cmake_minimum_required(VERSION 3.21)   # >=3.15 for CMP0091 (MSVC runtime abstraction) = NEW

# ---------------------------------------------------------------------------
# Plugin metadata — single source of truth (replaces the old vcxproj
# PluginInfo PropertyGroup). Change the version/description here.
# ---------------------------------------------------------------------------
project(ts3websitepreview
    VERSION 3.0
    DESCRIPTION "TeamSpeak 3 Website Preview plugin"
    LANGUAGES C RC)                    # RC enables the resource compiler for settings.rc

set(PLUGIN_NAME        "Website Preview")
set(PLUGIN_TYPE        "Plugin")
set(PLUGIN_VERSION     "${PROJECT_VERSION}")
set(PLUGIN_AUTHOR      "NobleUplift (Patrick Seiter)")
set(PLUGIN_DESCRIPTION "Fetches the title (and optionally description) of URLs sent in chat. Works for URL-only messages and for URLs embedded in typed messages.")

if(NOT MSVC)
    message(FATAL_ERROR "This plugin builds only with MSVC on Windows.")
endif()

# ---------------------------------------------------------------------------
# Architecture -> TS3 arch suffix + vendored lib dir.
# CMAKE_SIZEOF_VOID_P is set after the compiler probe and is generator-
# independent (works for the VS generator's -A Win32/x64 and for Ninja).
# TS3 ignores plugin DLLs whose name lacks the _win32 / _win64 suffix.
# ---------------------------------------------------------------------------
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TS3_ARCH_SUFFIX "win64")
    set(TP_LIBDIR_NAME  "lib64")
else()
    set(TS3_ARCH_SUFFIX "win32")
    set(TP_LIBDIR_NAME  "lib")
endif()
message(STATUS "Building TS3 plugin for ${TS3_ARCH_SUFFIX}")

# ---------------------------------------------------------------------------
# Dependency trees.
#   ts3client-pluginsdk/  : git submodule, provides the TS3 SDK headers.
#   third_party/          : vendored libcurl + libxml2 + iconv (gitignored,
#                           sourced externally). Only HEADERS are needed to
#                           build the plugin — the DLLs are loaded at runtime
#                           via LoadLibraryExW + GetProcAddress, so NO import
#                           libs are linked into the plugin.
# Expected third_party layout (mirrors the old ts3websitepreview/ vendor dirs):
#   third_party/include/curl/*.h
#   third_party/include/libxml/*.h
#   third_party/lib/    (win32: libcurl.dll libxml2.dll iconv.dll + matching .lib)
#   third_party/lib64/  (x64:   libcurl.dll libxml2.dll iconv.dll + matching .lib)
# ---------------------------------------------------------------------------
set(TS3_SDK_INCLUDE "${CMAKE_SOURCE_DIR}/ts3client-pluginsdk/include")
set(TP              "${CMAKE_SOURCE_DIR}/third_party")
set(TP_INC          "${TP}/include")
set(TP_LIBDIR       "${TP}/${TP_LIBDIR_NAME}")

if(NOT EXISTS "${TS3_SDK_INCLUDE}/ts3_functions.h")
    message(FATAL_ERROR "TS3 SDK header not found at ${TS3_SDK_INCLUDE}. Run: git submodule update --init")
endif()
if(NOT EXISTS "${TP_INC}/curl/curl.h")
    message(FATAL_ERROR "third_party headers missing at ${TP_INC} (need curl/ and libxml/). See README.")
endif()

# ---------------------------------------------------------------------------
# Generate plugin_version.h (replaces the vcxproj WriteLinesToFile target).
# configure_file runs at configure time, so it exists before the first build.
# (package.ini is a packaging concern handled by CI — deploy/package.ini carries
# a <version> placeholder for CI to substitute — so it is not generated here.)
# ---------------------------------------------------------------------------
configure_file("${CMAKE_SOURCE_DIR}/cmake/plugin_version.h.in"
               "${CMAKE_BINARY_DIR}/generated/plugin_version.h" @ONLY)

# ===========================================================================
#  The plugin DLL (MODULE = a runtime-loaded plugin, not linked against).
# ===========================================================================
add_library(ts3websitepreview MODULE
    src/core.c
    src/plugin.c
    src/settings.c
    src/settings.rc)

set_target_properties(ts3websitepreview PROPERTIES
    OUTPUT_NAME "ts3websitepreview_${TS3_ARCH_SUFFIX}"   # -> ts3websitepreview_win64.dll
    PREFIX ""
    SUFFIX ".dll"
    C_STANDARD 11
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"   # /MD (/MDd in Debug)
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE                          # WholeProgramOptimization
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"                 # CI collects the DLL from here
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/implib")            # keeps the import .lib/.exp out of it

target_include_directories(ts3websitepreview PRIVATE
    "${CMAKE_SOURCE_DIR}/src"          # resource.h + local headers (also visible to rc.exe)
    "${TS3_SDK_INCLUDE}"               # "teamspeak/...", "ts3_functions.h", "teamlog/..."
    "${TP_INC}"                        # so libxml2/curl internal <libxml/..>/<curl/..> resolve
    "${TP_INC}/curl"                   # #include "curl.h"
    "${TP_INC}/libxml"                 # #include "HTMLparser.h" "globals.h" "xpath.h"
    "${CMAKE_BINARY_DIR}/generated")   # #include "plugin_version.h"

target_compile_definitions(ts3websitepreview PRIVATE
    UNICODE _UNICODE
    WIN32 _WINDOWS _USRDLL WINDOWS
    TS3WEBSITEPREVIEW_EXPORTS)

# No curl/libxml2/iconv import libs: everything is LoadLibraryExW + GetProcAddress.
# kernel32/user32/gdi32/... come from MSVC's default standard libraries.

# ===========================================================================
#  Packaging into the installable .ts3_plugin (staging + zip) is intentionally
#  NOT done here — it is deferred to CI (a GitHub Actions workflow, to be added).
#  CI collects the DLL from build/<arch>/out/, adds the dependency DLLs and a
#  substituted package.ini, and zips it per the layout documented in CLAUDE.md.
# ===========================================================================

# ===========================================================================
#  Optional Unity C unit tests (console EXE). test_parse.c calls libxml2
#  DIRECTLY, so — unlike the plugin — this target LINKS the import libs.
#     cmake -S . -B build/win64 -A x64 -DBUILD_TESTS=ON
#     cmake --build build/win64 --config Release --target ts3websitepreview_tests
#     ctest --test-dir build/win64 -C Release
# ===========================================================================
option(BUILD_TESTS "Build the Unity C unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_executable(ts3websitepreview_tests
        test/main_test.c
        test/test_callback.c
        test/test_inline.c
        test/test_message.c
        test/test_parse.c
        test/test_url.c
        test/unity/unity.c
        src/core.c)                    # main.c is intentionally NOT built (has its own main())

    target_include_directories(ts3websitepreview_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/test"     # "unity/unity.h"
        "${CMAKE_SOURCE_DIR}/src"      # "core.h"
        "${TP_INC}" "${TP_INC}/curl" "${TP_INC}/libxml")

    target_compile_definitions(ts3websitepreview_tests PRIVATE WIN32 _CONSOLE)
    set_target_properties(ts3websitepreview_tests PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    set(_test_libs "${TP_LIBDIR}/libcurl.lib" "${TP_LIBDIR}/libxml2.lib" "${TP_LIBDIR}/iconv.lib")
    set(_test_dlls "${TP_LIBDIR}/libcurl.dll" "${TP_LIBDIR}/libxml2.dll")
    if(TS3_ARCH_SUFFIX STREQUAL "win32")
        list(APPEND _test_libs "${TP_LIBDIR}/zlib1.lib")   # win32 test linked zlib1 (x64 did not)
    endif()
    target_link_libraries(ts3websitepreview_tests PRIVATE ${_test_libs})

    # libxml2.dll imports libiconv.dll by that exact name -> copy iconv.dll as libiconv.dll
    # next to the EXE, plus the other runtime DLLs, so the tests can run.
    add_custom_command(TARGET ts3websitepreview_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_test_dlls} "$<TARGET_FILE_DIR:ts3websitepreview_tests>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP_LIBDIR}/iconv.dll" "$<TARGET_FILE_DIR:ts3websitepreview_tests>/libiconv.dll"
        VERBATIM)

    add_test(NAME unit COMMAND ts3websitepreview_tests)
endif()
