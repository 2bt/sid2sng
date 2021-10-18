cmake_minimum_required(VERSION 3.4.1)

project(sid2sng)

if (!MSVC)
    add_compile_options(-Wall -O3)
endif()

add_executable(${PROJECT_NAME}
    src/gsong.cpp
    src/gsong.hpp
    src/main.cpp
    )

