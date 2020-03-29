cmake_minimum_required(VERSION 3.15)
project(hyx_0_1_5 C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(hyx_0_1_5
        ansi.h
        blob.c
        blob.h
        common.c
        common.h
        history.c
        history.h
        hyx.c
        input.c
        input.h
        updater.c
        updater.h
        view.c
        view.h)
