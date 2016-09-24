cmake_minimum_required(VERSION 3.6)
project(hashids_c)

set(SOURCE_FILES
    src/hashids.c
    src/hashids.h
    src/test.c)

add_executable(unittest ${SOURCE_FILES})
