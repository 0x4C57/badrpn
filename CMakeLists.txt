cmake_minimum_required(VERSION 3.5)

project(badrpn LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(badrpn main.c)
