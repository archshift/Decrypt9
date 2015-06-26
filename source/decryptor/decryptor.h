#pragma once

#include "common.h"

#define MAX_ENTRIES 1024

typedef struct {
    u8   CTR[16];
    u32  size_mb;
    char filename[180];
} __attribute__((packed)) SdInfoEntry;

typedef struct {
    u32 n_entries;
    SdInfoEntry entries[MAX_ENTRIES];
} __attribute__((packed, aligned(16))) SdInfo;

typedef struct {
    u8   CTR[16];
    u8   keyY[16];
    u32  size_mb;
    u8   reserved[4];
    u32  usesSeedCrypto;
    u32  uses7xCrypto;
    u64  titleId;
    char filename[112];
} __attribute__((packed)) NcchInfoEntry;

typedef struct {
    u32 padding;
    u32 ncch_info_version;
    u32 n_entries;
    u8  reserved[4];
    NcchInfoEntry entries[MAX_ENTRIES];
} __attribute__((packed, aligned(16))) NcchInfo;

typedef struct {
    u64 titleId;
    u8 external_seed[16];
    u8 reserved[8];
} __attribute__((packed)) SeedInfoEntry;

typedef struct {
    u32 n_entries;
    u8 padding[12];
    SeedInfoEntry entries[MAX_ENTRIES];
} __attribute__((packed)) SeedInfo;

typedef struct {
    u32  keyslot;
    u32  setKeyY;
    u8   CTR[16];
    u8   keyY[16];
    u32  size;
    u8*  buffer;
} __attribute__((packed)) DecryptBufferInfo;

typedef struct {
    u32  keyslot;
    u32  setKeyY;
    u8   CTR[16];
    u8   keyY[16];
    u32  size_mb;
    char filename[180];
} __attribute__((packed, aligned(16))) PadInfo;

typedef struct {
    u32 commonKeyIndex;
    u8  reserved[4];
    u8  titleId[8];
    u8  encryptedTitleKey[16];
} __attribute__((packed)) TitleKeyEntry;

typedef struct {
    u32 n_entries;
    u8  reserved[12];
    TitleKeyEntry entries[MAX_ENTRIES];
} __attribute__((packed, aligned(16))) EncKeysInfo;

static u8* FindNandCtr();
u32 DumpPartition(char* filename, u32 offset, u32 size, u32 keyslot);
u32 DecryptBuffer(DecryptBufferInfo *info);
u32 CreatePad(PadInfo *info);

