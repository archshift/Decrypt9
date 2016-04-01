#include "heap.h"

#include <ncx_mempool/ncx_slab.h>

extern const u32 __heap_start;
extern const u32 __heap_size;

static ncx_slab_pool_t* pool = NULL;

// ncx_mempool requires this to be defined
int getpagesize() {
    return 4096;
}

bool InitHeap()
{
    if (pool != NULL) {
        return false;
    }
    pool = (ncx_slab_pool_t*)__heap_start;
    pool->addr = __heap_start;
	pool->min_shift = 3;
	pool->end = __heap_start + __heap_size;

    ncx_slab_init(pool);
}

void DeinitHeap()
{
    if (pool != NULL) {
        pool = NULL;
    }
}

void* MemAlloc(size_t amount)
{
    void* ptr = ncx_slab_alloc(pool, amount);
    // TODO: die if ptr is null
    return ptr;
}

void MemFree(void* mem)
{
    ncx_slab_free(pool, mem);
}