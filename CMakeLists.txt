cmake_minimum_required(VERSION 3.4.1)

project(sid2sng)
add_compile_options(-Wall -O3)


add_executable(${PROJECT_NAME}
    src/gsong.cpp
    src/gsong.hpp
    src/main.cpp
    )

