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
#define END_Y   (SCREEN_HEIGHT - 10)
#define START_X 10
#define END_X   (SCREEN_WIDTH - 10)

static int current_y = START_Y;

void ClearScreen(u8* screen, int color)
{
    for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i++) {
        *(screen++) = color >> 16;  // B
        *(screen++) = color >> 8;   // G
        *(screen++) = color & 0xFF; // R
    }
}

void DrawCharacter(u8* screen, int character, int x, int y, int color, int bgcolor)
{
    for (int yy = 0; yy < 8; yy++) {
        int xDisplacement = (x * BYTES_PER_PIXEL * SCREEN_HEIGHT);
        int yDisplacement = ((SCREEN_HEIGHT - (y + yy) - 1) * BYTES_PER_PIXEL);
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
            screenPos += BYTES_PER_PIXEL * SCREEN_HEIGHT;
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
    char str[256] = {};
    va_list va;

    va_start(va, format);
    vsnprintf(str, 256, format, va);
    va_end(va);

    DrawString(TOP_SCREEN0, str, x, y, FONT_COLOR, BG_COLOR);
    DrawString(TOP_SCREEN1, str, x, y, FONT_COLOR, BG_COLOR);
}

void DebugClear()
{
    ClearScreen(TOP_SCREEN0, BG_COLOR);
    ClearScreen(TOP_SCREEN1, BG_COLOR);
    current_y = START_Y;
}

void Debug(const char *format, ...)
{
    char str[256] = {};
    va_list va;

    va_start(va, format);
    vsnprintf(str, ((END_X - START_X) / 8) + 1, format, va);
    va_end(va);
    
    if (current_y >= END_Y) {
        DebugClear();
    }

    DrawString(TOP_SCREEN0, str, START_X, current_y, FONT_COLOR, BG_COLOR);
    DrawString(TOP_SCREEN1, str, START_X, current_y, FONT_COLOR, BG_COLOR);

    current_y += 10;
}

void ShowProgress(u64 current, u64 total)
{
    if (total > 0)
        DrawStringF(SCREEN_WIDTH - 40, SCREEN_HEIGHT - 20, "%3llu%%", (current * 100) / total);
    else
        DrawStringF(SCREEN_WIDTH - 40, SCREEN_HEIGHT - 20, "    ");
}
