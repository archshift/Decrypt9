#include "common.h"

bool InitHeap();
void DeinitHeap();

void* MemAlloc(size_t amount);
void MemFree(void* mem);