cmake_minimum_required(VERSION 3.5)

project(fs LANGUAGES C)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -pedantic -ansi")

add_executable(struktura
    src/main.c
)