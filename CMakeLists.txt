cmake_minimum_required(VERSION 3.20)
project(CinematicConversationCameraLE VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "CCC LE requires the Windows MSVC toolchain.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "CCC LE must be built as a 32-bit (Win32) plugin.")
endif()

set(SKSE_LE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../skse173/skse_1_07_03/src" CACHE PATH
    "Path to the SKSE 1.7.3 source archive")
if(NOT EXISTS "${SKSE_LE_ROOT}/skse/skse/PluginAPI.h")
    message(FATAL_ERROR "SKSE 1.7.3 headers not found at ${SKSE_LE_ROOT}")
endif()

add_library(CinematicConversationCameraLE SHARED
    src/Plugin.cpp
    src/CameraDirector.cpp
    src/Logger.cpp
    src/Plugin.def)

target_compile_features(CinematicConversationCameraLE PRIVATE cxx_std_17)
target_include_directories(CinematicConversationCameraLE PRIVATE
    "${SKSE_LE_ROOT}"
    "${SKSE_LE_ROOT}/skse"
    "${SKSE_LE_ROOT}/common")
target_compile_definitions(CinematicConversationCameraLE PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(CinematicConversationCameraLE PRIVATE /W4 /EHsc)
target_link_libraries(CinematicConversationCameraLE PRIVATE Shell32)
set_target_properties(CinematicConversationCameraLE PROPERTIES
    OUTPUT_NAME "CinematicConversationCamera")

