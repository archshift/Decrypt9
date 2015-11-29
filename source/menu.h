#pragma once

#include "common.h"

#define MENU_MAX_ENTRIES    12
#define MENU_MAX_DEPTH      4
typedef struct {
    char* name;
    u32 (*function)(u32 param);
    bool dangerous;
    bool emunand;
    u32 param;
} MenuEntry;

typedef struct {
    char* name;
    u32 n_entries;
    MenuEntry entries[MENU_MAX_ENTRIES];
} MenuInfo;

u32 ProcessMenu(MenuInfo* info, u32 n_entries_main);
