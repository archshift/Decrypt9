#pragma once

#include "common.h"

#define MAXENTRIES 1024
struct title_key_entry {
	u32 commonKeyIndex;
	u8  reserved[4];
	u8  titleId[8];
	u8  encryptedTitleKey[16];
} __attribute__((packed));

struct enckeys_info {
	u32 n_entries;
	u8  reserved[12];
	struct title_key_entry entries[MAXENTRIES];
} __attribute__((packed, aligned(16)));

u32 DecryptTitlekeys(void);
