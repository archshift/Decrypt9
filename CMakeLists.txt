cmake_minimum_required(VERSION 2.8.4)
include(ExternalProject)
project(Decrypt9)

# set(DECRYPT9_LOADER_CAKEHAX ON CACHE BOOL "Build CakeHax payload")
# set(DECRYPT9_LOADER_CAKESROP ON CACHE BOOL "Build CakesROP payload")
set(DECRYPT9_LOADER_BRAHMA ON CACHE BOOL "Build Brahma payload")
set(DECRYPT9_LOADER_BRAHMA2_STANDALONE ON CACHE BOOL "Build standalone Brahma2 app")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Os")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Os")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g -Wall -Os")

add_subdirectory(source)
