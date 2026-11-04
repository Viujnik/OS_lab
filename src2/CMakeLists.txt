cmake_minimum_required(VERSION 4.1)
project(os_lab C)

set(CMAKE_C_STANDARD 23)

add_executable(os_lab main.c
        child1.c
        child2.c
        main.c)
