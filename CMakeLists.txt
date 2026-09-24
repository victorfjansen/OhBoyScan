cmake_minimum_required(VERSION 3.21)
project(my_application LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Slint REQUIRED)

add_executable(my_application src/main.cpp)

target_link_libraries(my_application PRIVATE Slint::Slint)

slint_target_sources(my_application ui/app-window.slint LIBRARY_PATHS material=${CMAKE_CURRENT_SOURCE_DIR}/ui/material-1.0/material.slint)

install(TARGETS my_application DESTINATION bin)