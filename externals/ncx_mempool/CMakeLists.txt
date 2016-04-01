
set(SRCS
    ncx_mempool/ncx_slab.c
    )
set(HEADERS
    ncx_mempool/ncx_core.h
    ncx_mempool/ncx_lock.h
    ncx_mempool/ncx_log.h
    ncx_mempool/ncx_slab.h
    )

add_library(ncx_mempool ${SRCS} ${HEADERS})
set_property(TARGET ncx_mempool APPEND PROPERTY COMPILE_DEFINITIONS
    LOG_LEVEL=32 NCX_PTR_SIZE=4 PAGE_MERGE)