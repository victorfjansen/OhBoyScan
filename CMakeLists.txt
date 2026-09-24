cmake_minimum_required(VERSION 3.21)
project(my_application LANGUAGES CXX)

# O Slint exige o padrão C++20 para compilar os bindings corretamente
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Força o CMake a usar o pacote Slint injetado pelo sysroot do Yocto. 
# Se não encontrar, ele aborta o build na hora, evitando falsos positivos.
find_package(Slint REQUIRED)

add_executable(my_application src/main.cpp)
target_link_libraries(my_application PRIVATE Slint::Slint)

# Transpila a interface utilizando o slint-compiler-native isolado
slint_target_sources(my_application ui/app-window.slint LIBRARY_PATHS material=${CMAKE_CURRENT_SOURCE_DIR}/ui/material-1.0/material.slint)

# Instala automaticamente no rootfs sem a necessidade de condicionais (IF)
install(TARGETS my_application DESTINATION bin)