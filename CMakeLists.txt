cmake_minimum_required(VERSION 3.21)   # >=3.15 for CMP0091 (MSVC runtime abstraction) = NEW

# ---------------------------------------------------------------------------
# Plugin metadata — single source of truth (replaces the old vcxproj
# PluginInfo PropertyGroup). Change the version/description here.
# PLUGIN_VERSION is a cache var so CI can override it from the git tag:
#   cmake -DPLUGIN_VERSION=1.2.3 ...
# ---------------------------------------------------------------------------
project(ts3websitepreview
    VERSION 3.0
    DESCRIPTION "TeamSpeak 3 Website Preview plugin"
    LANGUAGES C)

set(PLUGIN_NAME        "Website Preview")
set(PLUGIN_TYPE        "Plugin")
set(PLUGIN_VERSION     "${PROJECT_VERSION}" CACHE STRING "Plugin version baked into the binary")
set(PLUGIN_AUTHOR      "NobleUplift (Patrick Seiter)")
set(PLUGIN_DESCRIPTION "Fetches the title (and optionally description) of URLs sent in chat. Works for URL-only messages and for URLs embedded in typed messages.")

if(WIN32)
    enable_language(RC)                # resource compiler for settings.rc (Windows only)
endif()

# ---------------------------------------------------------------------------
# Platform -> TS3 binary suffix + file extension.
# TS3 loads a plugin by its filename suffix, so the binary must be named
# ts3websitepreview_<suffix>.<ext> exactly.
# ---------------------------------------------------------------------------
if(APPLE)
    set(TS3_ARCH_SUFFIX "mac")
    set(TS3_LIB_EXT     ".dylib")
elseif(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(TS3_ARCH_SUFFIX "win64")
        set(TP_LIBDIR_NAME  "lib64")
    else()
        set(TS3_ARCH_SUFFIX "win32")
        set(TP_LIBDIR_NAME  "lib")
    endif()
    set(TS3_LIB_EXT ".dll")
else()  # Linux / other Unix
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(TS3_ARCH_SUFFIX "linux_amd64")
    else()
        set(TS3_ARCH_SUFFIX "linux_x86")
    endif()
    set(TS3_LIB_EXT ".so")
endif()
message(STATUS "Building TS3 plugin for ${TS3_ARCH_SUFFIX}")

# ---------------------------------------------------------------------------
# TS3 SDK headers come from the git submodule on every platform.
# ---------------------------------------------------------------------------
set(TS3_SDK_INCLUDE "${CMAKE_SOURCE_DIR}/ts3client-pluginsdk/include")
if(NOT EXISTS "${TS3_SDK_INCLUDE}/ts3_functions.h")
    message(FATAL_ERROR "TS3 SDK header not found at ${TS3_SDK_INCLUDE}. Run: git submodule update --init")
endif()

# ---------------------------------------------------------------------------
# libcurl + libxml2.
#   Windows: the plugin loads the DLLs at runtime (LoadLibraryExW +
#            GetProcAddress) and links NO import libs — it only needs HEADERS.
#            Locally those come from the hand-supplied, gitignored third_party/
#            tree; in CI (where third_party/ is absent) they come from vcpkg via
#            find_package. Expected local layout:
#              third_party/include/{curl,libxml}/*.h
#              third_party/{lib,lib64}/  (libcurl/libxml2/iconv .dll + .lib)
#   Linux/macOS: system libcurl + libxml2, linked directly (no dlopen).
# ---------------------------------------------------------------------------
set(TP        "${CMAKE_SOURCE_DIR}/third_party")
set(TP_INC    "${TP}/include")
if(WIN32)
    set(TP_LIBDIR "${TP}/${TP_LIBDIR_NAME}")
endif()
if(WIN32 AND EXISTS "${TP_INC}/curl/curl.h")
    set(USE_THIRD_PARTY TRUE)   # local Windows dev with vendored deps
else()
    set(USE_THIRD_PARTY FALSE)  # Linux/macOS system libs, or Windows CI/vcpkg
    find_package(CURL REQUIRED)
    find_package(LibXml2 REQUIRED)
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
#  The plugin (MODULE = a runtime-loaded plugin, not linked against).
# ===========================================================================
set(PLUGIN_SOURCES src/core.c src/plugin.c src/settings.c)
if(WIN32)
    list(APPEND PLUGIN_SOURCES src/settings.rc)
endif()
add_library(ts3websitepreview MODULE ${PLUGIN_SOURCES})

set_target_properties(ts3websitepreview PROPERTIES
    OUTPUT_NAME "ts3websitepreview_${TS3_ARCH_SUFFIX}"   # -> ts3websitepreview_win64.dll / _linux_amd64.so / _mac.dylib
    PREFIX ""
    SUFFIX "${TS3_LIB_EXT}"
    C_STANDARD 11
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"   # Windows .dll lands here
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")  # Linux/macOS .so/.dylib lands here

target_include_directories(ts3websitepreview PRIVATE
    "${CMAKE_SOURCE_DIR}/src"          # resource.h + local headers
    "${TS3_SDK_INCLUDE}"               # "teamspeak/...", "ts3_functions.h", "teamlog/..."
    "${CMAKE_BINARY_DIR}/generated")   # "plugin_version.h"

if(WIN32)
    set_target_properties(ts3websitepreview PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"   # /MD (/MDd in Debug)
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE                          # WholeProgramOptimization
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/implib")            # import .lib/.exp away from the DLL
    target_compile_definitions(ts3websitepreview PRIVATE
        UNICODE _UNICODE WIN32 _WINDOWS _USRDLL WINDOWS TS3WEBSITEPREVIEW_EXPORTS)
    # Headers only — the DLLs are loaded at runtime, so link NO curl/libxml2 import libs.
    if(USE_THIRD_PARTY)
        target_include_directories(ts3websitepreview PRIVATE "${TP_INC}" "${TP_INC}/curl" "${TP_INC}/libxml")
    else()
        target_include_directories(ts3websitepreview PRIVATE ${CURL_INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIR})
    endif()
else()
    # Linux/macOS: link the system libraries directly (their targets carry the include dirs).
    target_compile_definitions(ts3websitepreview PRIVATE TS3WEBSITEPREVIEW_EXPORTS)
    target_link_libraries(ts3websitepreview PRIVATE CURL::libcurl LibXml2::LibXml2)
endif()

# ===========================================================================
#  Packaging into the installable .ts3_plugin (staging + zip) is intentionally
#  NOT done here — it is handled by CI (.github/workflows/deploy.yml). CI
#  collects the per-platform binaries from build/**/out/, adds the Windows
#  dependency DLLs + a substituted package.ini, and zips them per CLAUDE.md.
# ===========================================================================

# ===========================================================================
#  Optional Unity C unit tests (console EXE). test_parse.c calls libxml2
#  DIRECTLY, so — unlike the Windows plugin — this target LINKS libxml2.
#     cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build && ctest --test-dir build
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
        "${CMAKE_SOURCE_DIR}/src")     # "core.h"
    if(WIN32)
        target_compile_definitions(ts3websitepreview_tests PRIVATE WIN32 _CONSOLE)
        set_target_properties(ts3websitepreview_tests PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()

    if(USE_THIRD_PARTY)
        # Local Windows dev: link the vendored import libs + copy runtime DLLs beside the EXE.
        target_include_directories(ts3websitepreview_tests PRIVATE "${TP_INC}" "${TP_INC}/curl" "${TP_INC}/libxml")
        set(_test_libs "${TP_LIBDIR}/libcurl.lib" "${TP_LIBDIR}/libxml2.lib" "${TP_LIBDIR}/iconv.lib")
        set(_test_dlls "${TP_LIBDIR}/libcurl.dll" "${TP_LIBDIR}/libxml2.dll")
        if(TS3_ARCH_SUFFIX STREQUAL "win32")
            list(APPEND _test_libs "${TP_LIBDIR}/zlib1.lib")
        endif()
        target_link_libraries(ts3websitepreview_tests PRIVATE ${_test_libs})
        add_custom_command(TARGET ts3websitepreview_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_test_dlls} "$<TARGET_FILE_DIR:ts3websitepreview_tests>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP_LIBDIR}/iconv.dll" "$<TARGET_FILE_DIR:ts3websitepreview_tests>/libiconv.dll"
            VERBATIM)
    else()
        # Linux/macOS (and Windows CI/vcpkg): link the resolved library targets.
        target_link_libraries(ts3websitepreview_tests PRIVATE CURL::libcurl LibXml2::LibXml2)
    endif()

    add_test(NAME unit COMMAND ts3websitepreview_tests)
endif()
