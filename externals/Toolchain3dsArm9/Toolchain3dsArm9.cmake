set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR armv5te)
set(3DS TRUE) # To be used for multiplatform projects

# DevkitPro Paths are broken on windows, so we have to fix those
macro(msys_to_cmake_path MsysPath ResultingPath)
    if(WIN32)
        string(REGEX REPLACE "^/([a-zA-Z])/" "\\1:/" ${ResultingPath} "${MsysPath}")
    else()
        set(${ResultingPath} "${MsysPath}")
    endif()
endmacro()

msys_to_cmake_path("$ENV{DEVKITPRO}" DEVKITPRO)
if(NOT IS_DIRECTORY ${DEVKITPRO})
    message(FATAL_ERROR "Please set DEVKITPRO in your environment")
endif()

msys_to_cmake_path("$ENV{DEVKITARM}" DEVKITARM)
if(NOT IS_DIRECTORY ${DEVKITARM})
    message(FATAL_ERROR "Please set DEVKITARM in your environment")
endif()

# Prefix detection only works with compiler id "GNU"
# CMake will look for prefixed g++, cpp, ld, etc. automatically
if(WIN32)
    set(CMAKE_ASM_COMPILER "${DEVKITARM}/bin/arm-none-eabi-gcc.exe")
    set(CMAKE_C_COMPILER "${DEVKITARM}/bin/arm-none-eabi-gcc.exe")
    set(CMAKE_CXX_COMPILER "${DEVKITARM}/bin/arm-none-eabi-g++.exe")
    set(CMAKE_AR "${DEVKITARM}/bin/arm-none-eabi-gcc-ar.exe" CACHE STRING "")
    set(CMAKE_RANLIB "${DEVKITARM}/bin/arm-none-eabi-gcc-ranlib.exe" CACHE STRING "")
    set(DKA_OBJCOPY "${DEVKITARM}/bin/arm-none-eabi-objcopy.exe")
else()
    set(CMAKE_ASM_COMPILER "${DEVKITARM}/bin/arm-none-eabi-gcc")
    set(CMAKE_C_COMPILER "${DEVKITARM}/bin/arm-none-eabi-gcc")
    set(CMAKE_CXX_COMPILER "${DEVKITARM}/bin/arm-none-eabi-g++")
    set(CMAKE_AR "${DEVKITARM}/bin/arm-none-eabi-gcc-ar" CACHE STRING "")
    set(CMAKE_RANLIB "${DEVKITARM}/bin/arm-none-eabi-gcc-ranlib" CACHE STRING "")
    set(DKA_OBJCOPY "${DEVKITARM}/bin/arm-none-eabi-objcopy")
endif()

set(CMAKE_FIND_ROOT_PATH ${DEVKITARM} ${DEVKITPRO})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

SET(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Shared libs not available")

add_definitions(-DARM9)

set(ARCH "-mthumb -mthumb-interwork -march=armv5te -mtune=arm946e-s")
set(CMAKE_C_FLAGS "${ARCH} -fomit-frame-pointer -ffast-math" CACHE STRING "C flags")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions" CACHE STRING "C++ flags")
set(CMAKE_ASM_FLAGS "${ARCH} -x assembler-with-cpp" CACHE STRING "ASM flags")

set(DKA_LDFLAGS "-nostartfiles ${ARCH}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${DKA_LDFLAGS}" CACHE STRING "Linker flags (EXE)")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${DKA_LDFLAGS}" CACHE STRING "Linker flags (module)")