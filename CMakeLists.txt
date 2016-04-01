cmake_minimum_required(VERSION 3.1.0)
include(ExternalProject)
project(Decrypt9)

# set(DECRYPT9_LOADER_CAKEHAX ON CACHE BOOL "Build CakeHax payload")
# set(DECRYPT9_LOADER_CAKESROP ON CACHE BOOL "Build CakesROP payload")
set(DECRYPT9_LOADER_BRAHMA ON CACHE BOOL "Build Brahma payload")
set(DECRYPT9_LOADER_BRAHMA2_STANDALONE ON CACHE BOOL "Build standalone Brahma2 app")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Os")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Os")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g -Wall -Os")

set(NCX_MEMPOOL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/externals/ncx_mempool")
include_directories(${NCX_MEMPOOL_PREFIX})
add_subdirectory(${NCX_MEMPOOL_PREFIX})

add_subdirectory(source)
