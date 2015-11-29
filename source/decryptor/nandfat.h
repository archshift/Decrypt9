#pragma once

#include "decryptor/nand.h"
#include "common.h"

#define MAX_ENTRIES 1024


u32 SeekFileInNand(u32* offset, u32* size, const char* path, PartitionInfo* partition);
// --> FEATURE FUNCTIONS <--
u32 DumpSeedsave(u32 param);
u32 UpdateSeedDb(u32 param);
u32 DumpTicket(u32 param);
