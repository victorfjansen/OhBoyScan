cmake_minimum_required(VERSION 3.21)
project(my_application LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Slint REQUIRED)

add_executable(my_application src/main.cpp)

target_link_libraries(my_application PRIVATE Slint::Slint)

if (WIN32)
    add_custom_command(TARGET my_application POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:my_application> $<TARGET_FILE_DIR:my_application> COMMAND_EXPAND_LISTS)
endif()

slint_target_sources(my_application ui/app-window.slint LIBRARY_PATHS material=${CMAKE_CURRENT_SOURCE_DIR}/ui/material-1.0/material.slint)

install(TARGETS my_application DESTINATION bin)