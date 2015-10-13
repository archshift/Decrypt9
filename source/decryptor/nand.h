#pragma once

#include "common.h"

#define NAND_SECTOR_SIZE 0x200
#define SECTORS_PER_READ (BUFFER_MAX_SIZE / NAND_SECTOR_SIZE)

#define P_TWLN    0
#define P_TWLP    1
#define P_AGBSAVE 2
#define P_FIRM0   3
#define P_FIRM1   4
#define P_CTRNAND 5

typedef struct {
    char name[16];
    u8  magic[8];
    u32 offset;
    u32 size;
    u32 keyslot;
    u32 mode;
} __attribute__((packed)) PartitionInfo;

bool IsEmuNand();
PartitionInfo* GetPartitionInfo(u32 partition_id);
u32 GetNandCtr(u8* ctr, u32 offset);

u32 DecryptNandToMem(u8* buffer, u32 offset, u32 size, PartitionInfo* partition);
u32 DecryptNandToFile(const char* filename, u32 offset, u32 size, PartitionInfo* partition);
u32 DecryptNandPartition(PartitionInfo* p);

u32 EncryptMemToNand(u8* buffer, u32 offset, u32 size, PartitionInfo* partition);
u32 EncryptFileToNand(const char* filename, u32 offset, u32 size, PartitionInfo* partition);
u32 InjectNandPartition(PartitionInfo* p);
