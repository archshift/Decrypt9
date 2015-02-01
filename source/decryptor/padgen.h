#pragma once

#include "common.h"

#define MAXENTRIES 1024

struct sd_info_entry {
    u8 CTR[16];
    u32 size_mb;
    char filename[180];
} __attribute__((packed));

struct sd_info {
    u32 n_entries;
    struct sd_info_entry entries[MAXENTRIES];
} __attribute__((packed, aligned(16)));


struct ncch_info_entry {
    u8  CTR[16];
    u8  keyY[16];
    u32 size_mb;
    u8  reserved[8];
    u32 uses7xCrypto;
    char filename[112];
} __attribute__((packed));

struct ncch_info {
    u32 padding;
    u32 ncch_info_version;
    u32 n_entries;
    u8  reserved[4];
    struct ncch_info_entry entries[MAXENTRIES];
} __attribute__((packed, aligned(16)));


struct pad_info {
    u32 keyslot;
    u32 setKeyY;
    u8 CTR[16];
    u8  keyY[16];
    u32 size_mb;
    char filename[180];
} __attribute__((packed, aligned(16)));


u32 ncchPadgen(void);
u32 sdPadgen(void);
u32 nandPadgen(void);

u32 createPad(struct pad_info *info);
