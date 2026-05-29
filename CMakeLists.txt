cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

# ---------------------------------------------------------------------------------------
# Project
#
# Thin C++20-module wrappers over the OpenGL ES 2.0 / 3.0 APIs (plus an optional
# KHR_debug helper). The compiled payload is intentionally tiny — the real
# interface is the module BMI, which every consumer recompiles from the shipped
# module sources against their own toolchain. There is therefore no portable
# binary to distribute and no ABI to stabilise, so the library is always STATIC.
# ---------------------------------------------------------------------------------------
project(gles_modules
    VERSION 0.1.0
    DESCRIPTION "C++20 module wrappers for OpenGL ES 2/3"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------------------
# C++ standard (modules require C++20 or newer)
# ---------------------------------------------------------------------------------------
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# NOTE: CMAKE_CXX_EXTENSIONS is deliberately left untouched. Named modules work
# with or without GNU extensions, but the BMI producer and consumer must agree
# on the setting. By not forcing it, our module units compile with the same
# dialect as the consuming build (which usually leaves the default), so the BMIs
# match. (Forcing it OFF here is what causes "configuration mismatch" errors for
# default consumers — and would only ever be required if this library adopted
# `import std;`, which it does not.)

if(CMAKE_CXX_STANDARD LESS 20)
    message(FATAL_ERROR
        "gles_modules: CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD} is too low"
        "C++20 modules require C++20 or newer.")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------------------
# Compiler support check
#
# Named modules need a recent Clang or GCC. The OpenGL::EGL / OpenGL::GLES*
# imported targets used below are only provided by CMake's FindOpenGL on Unix,
# so non-Unix targets (Windows/macOS via ANGLE, etc.) need a toolchain that
# supplies those libraries another way — see README.
# ---------------------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16")
        message(FATAL_ERROR "gles_modules: Clang >= 16 required for C++20 modules "
            "(found ${CMAKE_CXX_COMPILER_VERSION}).")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14")
        message(FATAL_ERROR "gles_modules: GCC >= 14 required for C++20 modules "
            "(found ${CMAKE_CXX_COMPILER_VERSION}).")
    endif()
else()
    message(WARNING "gles_modules: untested compiler '${CMAKE_CXX_COMPILER_ID}'. "
        "Clang >= 16 or GCC >= 14 are the supported toolchains.")
endif()

if(NOT UNIX)
    message(WARNING "gles_modules: CMake's FindOpenGL only provides the EGL/GLES "
        "imported targets on Unix. On this platform you must supply OpenGL ES "
        "(e.g. ANGLE) yourself — see README.")
endif()

# ---------------------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------------------
option(GLES_MODULES_ES3   "Build the GLES 3.0 module (gles.gl3) on top of GLES 2.0" ON)
option(GLES_MODULES_DEBUG "Enable the KHR_debug helper (links EGL for the default loader)" OFF)
option(GLES_MODULES_PIC   "Build with position-independent code" OFF)
option(GLES_MODULES_INSTALL "Generate install + find_package() rules" "${PROJECT_IS_TOP_LEVEL}")

# ---------------------------------------------------------------------------------------
# Library (always static, see header note)
# ---------------------------------------------------------------------------------------
add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

if(GLES_MODULES_PIC)
    set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# ---------------------------------------------------------------------------------------
# Module interface units
# ---------------------------------------------------------------------------------------
set(ModuleInterfaceFiles gles2.cppm gles_debug.cppm)

if(GLES_MODULES_ES3)
    list(APPEND ModuleInterfaceFiles gles3.cppm)
endif()

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET gles_modules_cxx TYPE CXX_MODULES
      BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
      FILES ${ModuleInterfaceFiles}
)

# ---------------------------------------------------------------------------------------
# OpenGL ES / EGL dependencies
#
# The core modules need only the GLES API. EGL is used solely for the debug
# helper's default loader (eglGetProcAddress) and is not present on every GLES
# platform (e.g. iOS uses EAGL, an unrelated API), so it is optional: when found
# it enables the convenience overload; otherwise the helper is reachable only
# through gl::enable_debug(loader).
# ---------------------------------------------------------------------------------------
if(GLES_MODULES_ES3)
    set(_gles_api GLES3)
else()
    set(_gles_api GLES2)
endif()

if(GLES_MODULES_DEBUG)
    set(_gles_optional EGL)
else()
    set(_gles_optional "")
endif()

find_package(OpenGL REQUIRED COMPONENTS ${_gles_api} OPTIONAL_COMPONENTS ${_gles_optional})

target_link_libraries(${PROJECT_NAME} PUBLIC OpenGL::${_gles_api})

if(GLES_MODULES_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GLES_MODULES_DEBUG)

    if(TARGET OpenGL::EGL)
        target_compile_definitions(${PROJECT_NAME} PRIVATE GLES_MODULES_EGL)
        target_link_libraries(${PROJECT_NAME} PUBLIC OpenGL::EGL)
    else()
        message(STATUS "gles_modules: EGL not found — the debug helper is available "
            "via gl::enable_debug(loader) only (no eglGetProcAddress convenience).")
    endif()
endif()

# ---------------------------------------------------------------------------------------
# Install + find_package(gles_modules) support
#
# Installs the module sources (consumers recompile the BMI with a compatible
# toolchain) along with the static archive and a package config. FetchContent /
# add_subdirectory remains the most robust way to consume a module library.
# ---------------------------------------------------------------------------------------
if(GLES_MODULES_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(_gles_modules_moddir ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/cxxmodules)

    # cmake-lint: disable=E1122
    # (false positive: ARCHIVE and FILE_SET are separate install(TARGETS)
    # artifact groups, each with its own DESTINATION, not a duplicate keyword)
    install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILE_SET gles_modules_cxx DESTINATION ${_gles_modules_moddir}
    )

    install(EXPORT ${PROJECT_NAME}-targets
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
        FILE ${PROJECT_NAME}-targets.cmake
        CXX_MODULES_DIRECTORY cxx-modules
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )
endif()
