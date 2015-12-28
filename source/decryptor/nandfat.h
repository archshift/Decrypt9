#pragma once

#include "decryptor/nand.h"
#include "common.h"

#define MAX_ENTRIES 1024

#define F_TICKET      (1<<0)
#define F_TITLE       (1<<1)
#define F_IMPORT      (1<<2)
#define F_CERTS       (1<<3)
#define F_SECUREINFO  (1<<4)
#define F_LOCALFRIEND (1<<5)
#define F_RANDSEED    (1<<6)
#define F_MOVABLE     (1<<7)
#define F_SEEDSAVE    (1<<8)

typedef struct {
    char name_sys[32];
    char name_emu[32];
    char path[64];
    u32 partition_id;
} NandFileInfo;

u32 SeekFileInNand(u32* offset, u32* size, const char* path, PartitionInfo* partition);
u32 DebugSeekFileInNand(u32* offset, u32* size, const char* filename, const char* path, PartitionInfo* partition);
// --> FEATURE FUNCTIONS <--
u32 DumpFile(u32 param);
u32 InjectFile(u32 param);
