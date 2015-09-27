#pragma once

#include "common.h"

typedef struct {
    char* name;
    u32 (*function)(void);
    u32 dangerous;
} MenuEntry;

typedef struct {
    char* name;
    MenuEntry entries[4];
} MenuInfo;

void ProcessMenu(MenuInfo* info, u32 nMenus);
