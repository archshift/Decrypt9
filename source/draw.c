// Copyright 2013 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "font.h"
#include "draw.h"

#define BG_COLOR   (RGB(0x00, 0x00, 0x00))
#define FONT_COLOR (RGB(0xFF, 0xFF, 0xFF))

#define START_Y 10
#define END_Y   230

static int current_y = START_Y;

void ClearScreen(u8* screen, int color)
{
    for (int i = 0; i < (SCREEN_HEIGHT * SCREEN_WIDTH); i++) {
        *(screen++) = color >> 16;  // B
        *(screen++) = color >> 8;   // G
        *(screen++) = color & 0xFF; // R
    }
}

void DrawCharacter(u8* screen, int character, int x, int y, int color, int bgcolor)
{
    for (int yy = 0; yy < 8; yy++) {
        int xDisplacement = (x * BYTES_PER_PIXEL * SCREEN_WIDTH);
        int yDisplacement = ((SCREEN_WIDTH - (y + yy) - 1) * BYTES_PER_PIXEL);
        u8* screenPos = screen + xDisplacement + yDisplacement;

        u8 charPos = font[character * 8 + yy];
        for (int xx = 7; xx >= 0; xx--) {
            if ((charPos >> xx) & 1) {
                *(screenPos + 0) = color >> 16;  // B
                *(screenPos + 1) = color >> 8;   // G
                *(screenPos + 2) = color & 0xFF; // R
            } else {
                *(screenPos + 0) = bgcolor >> 16;  // B
                *(screenPos + 1) = bgcolor >> 8;   // G
                *(screenPos + 2) = bgcolor & 0xFF; // R
            }
            screenPos += BYTES_PER_PIXEL * SCREEN_WIDTH;
        }
    }
}

void DrawString(u8* screen, const char *str, int x, int y, int color, int bgcolor)
{
    for (int i = 0; i < strlen(str); i++)
        DrawCharacter(screen, str[i], x + i * 8, y, color, bgcolor);
}

void DrawStringF(int x, int y, const char *format, ...)
{
    char* str;
    va_list va;

    va_start(va, format);
    vasprintf(&str, format, va);
    va_end(va);

    DrawString(TOP_SCREEN, str, x, y, FONT_COLOR, BG_COLOR);
    free(str);
}

void DebugClear()
{
    ClearScreen(TOP_SCREEN, BG_COLOR);
    current_y = START_Y;
}

void Debug(const char *format, ...)
{
    char* str;
    va_list va;

    va_start(va, format);
    vasprintf(&str, format, va);
    va_end(va);

    DrawString(TOP_SCREEN, str, 10, current_y, FONT_COLOR, BG_COLOR);
    free(str);

    current_y += 10;
}

void ShowProgress(u32 current, u32 total)
{
    if (total > 0)
        DrawStringF(SCREEN_HEIGHT - 40, SCREEN_WIDTH - 20, "%i%%", current / (total/100));
    else
        DrawStringF(SCREEN_HEIGHT - 40, SCREEN_WIDTH - 20, "    ");
}
