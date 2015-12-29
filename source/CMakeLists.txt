include_directories(".")
enable_language(ASM)

set(SOURCE_FILES
    decryptor/aes.c
    decryptor/decryptor.c
    decryptor/game.c
    decryptor/nand.c
    decryptor/nandfat.c
    decryptor/sha.c
    decryptor/titlekey.c
    draw.c
    fatfs/delay.s
    fatfs/diskio.c
    fatfs/ff.c
    fatfs/sdmmc.c
    fs.c
    hid.c
    i2c.c
    main.c
    menu.c
    platform.c
    )

set(HEADER_FILES
    common.h
    decryptor/aes.h
    decryptor/decryptor.h
    decryptor/game.h
    decryptor/nandfat.h
    decryptor/nand.h
    decryptor/sha.h
    decryptor/titlekey.h
    draw.h
    fatfs/delay.h
    fatfs/diskio.h
    fatfs/ffconf.h
    fatfs/ff.h
    fatfs/integer.h
    fatfs/sdmmc.h
    font.h
    fs.h
    hid.h
    i2c.h
    menu.h
    platform.h
    )

set(BS_SOURCE_FILES
    loader-bs/bs-start.s
    )
set(GW_SOURCE_FILES
    loader-gw/gw-start.s
    )

set(TARGET_NAME_BS decrypt9-bs)
set(TARGET_NAME_GW decrypt9-gw)

function(add_output_bin TARGET_NAME TARGET_DEFINITION)
    add_executable(${TARGET_NAME} ${ARGN})
    set_property(TARGET ${TARGET_NAME} APPEND PROPERTY COMPILE_DEFINITIONS ${TARGET_DEFINITION})
    add_custom_command(OUTPUT ${TARGET_NAME}.bin
        COMMAND ${DKA_OBJCOPY} --set-section-flags .bss=alloc,load,contents -O binary
            $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.bin
        DEPENDS $<TARGET_FILE:${TARGET_NAME}> VERBATIM)
endfunction()

# if(DECRYPT9_LOADER_CAKEHAX)
#     set(EXEC_GW ON)
# endif()

# if(DECRYPT9_LOADER_CAKESROP)
#     set(EXEC_GW ON)
# endif()

if(DECRYPT9_LOADER_BRAHMA)
    set(EXEC_BS ON)
endif()

if(DECRYPT9_LOADER_BRAHMA2_STANDALONE)
    set(EXEC_BS ON)
    set(BRAHMA2_APP_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Decrypt9.3dsx)
    set(BRAHMA2_SMDH_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Decrypt9.smdh)
    add_custom_command(OUTPUT ${BRAHMA2_APP_OUTPUT} ${BRAHMA2_SMDH_OUTPUT}
        COMMAND ${CMAKE_SOURCE_DIR}/externals/brahma-loader-build.sh
            ${CMAKE_CURRENT_BINARY_DIR} ${TARGET_NAME_BS} ${CMAKE_SOURCE_DIR}/resources/BrahmaAppInfo
        DEPENDS ${TARGET_NAME_BS}.bin VERBATIM)
    add_custom_target(decrypt9_3dsx ALL SOURCES ${BRAHMA2_APP_OUTPUT})
endif()

if(EXEC_BS)
    set(LD_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/loader-bs/bootstrap.ld")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-T${LD_FILE_PATH}")
    add_output_bin(decrypt9-bs EXEC_BOOTSTRAP ${SOURCE_FILES} ${HEADER_FILES} ${BS_SOURCE_FILES})
endif()

if(EXEC_GW)
    set(LD_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/loader-bs/bootstrap.ld")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-T${LD_FILE_PATH}")
    add_output_bin(decrypt9-gw EXEC_GATEWAY "${CMAKE_CURRENT_SOURCE_DIR}/loader-gw/gateway.ld"
        ${SOURCE_FILES} ${HEADER_FILES} ${GW_SOURCE_FILES})
endif()
