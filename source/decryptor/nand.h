#pragma once

#include "common.h"

#define NAND_SECTOR_SIZE 0x200
#define SECTORS_PER_READ (BUFFER_MAX_SIZE / NAND_SECTOR_SIZE)

#define P_TWLN    (1<<0)
#define P_TWLP    (1<<1)
#define P_AGBSAVE (1<<2)
#define P_FIRM0   (1<<3)
#define P_FIRM1   (1<<4)
#define P_CTRNAND (1<<5)
#define P_ALL     (P_TWLN | P_TWLP | P_AGBSAVE | P_FIRM0 | P_FIRM1 | P_CTRNAND)

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

// --> FEATURE FUNCTIONS <--
u32 SetNand(bool use_emunand);

u32 CtrNandPadgen(u32 param);
u32 TwlNandPadgen(u32 param);

u32 DumpNand(u32 param);
u32 RestoreNand(u32 param);
u32 DecryptNandPartitions(u32 param);
u32 InjectNandPartitions(u32 param);
