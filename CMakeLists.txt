cmake_minimum_required(VERSION 3.6)
project(hashids_c)
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS Release "-std=c99 -O3")
set(CMAKE_CXX_FLAGS Debug "-std=c99 -g -Oz")

set(SOURCE_FILES
    hashids.c
    hashids.h
    test.c)

add_executable(unittest ${SOURCE_FILES})
