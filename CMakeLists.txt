cmake_minimum_required(VERSION 3.21)
project(my_application LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Slint QUIET)
if (NOT Slint_FOUND)
  message("Slint could not be located in the CMake module search path. Downloading it from Git and building it locally")
  include(FetchContent)
  FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    # `release/1` will auto-upgrade to the latest Slint >= 1.0.0 and < 2.0.0
    # `release/1.0` will auto-upgrade to the latest Slint >= 1.0.0 and < 1.1.0
    GIT_TAG release/1
    SOURCE_SUBDIR api/cpp
  )
  FetchContent_MakeAvailable(Slint)
endif (NOT Slint_FOUND)

set(APP_SOURCES
    src/main.cpp
    src/mikrotik_device.cpp
    src/routeros_api_client.cpp
    src/scan_engine.cpp
    src/gateway_detector.cpp
    src/exposure_checker.cpp
    src/topology_scanner.cpp
    src/ecdsa_validator.cpp
    src/arp_monitor.cpp
    src/cable_tester.cpp
)

if(ANDROID)
    add_library(my_application SHARED ${APP_SOURCES})
else()
    find_package(CURL REQUIRED)
    find_package(OpenSSL REQUIRED)
    add_executable(my_application ${APP_SOURCES})
endif()

target_link_libraries(my_application PRIVATE Slint::Slint)
if(NOT ANDROID)
    target_link_libraries(my_application PRIVATE CURL::libcurl OpenSSL::Crypto)
endif()

slint_target_sources(my_application ui/app-window.slint LIBRARY_PATHS material=${CMAKE_CURRENT_SOURCE_DIR}/ui/material-1.0/material.slint)

# On Windows, copy the Slint DLL next to the application binary so that it's found.
if (WIN32)
    add_custom_command(TARGET my_application POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:my_application> $<TARGET_FILE_DIR:my_application> COMMAND_EXPAND_LISTS)
endif()