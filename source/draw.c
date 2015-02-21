#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "font.h"
#include "draw.h"

int current_y = 0;

void ClearScreen(unsigned char *screen, int color)
{
    int i;
    unsigned char *screenPos = screen;
    for (i = 0; i < (SCREEN_HEIGHT * SCREEN_WIDTH); i++)
    {
        *(screenPos++) = color >> 16; //B
        *(screenPos++) = color >> 8; //G
        *(screenPos++) = color & 0xFF; //R
    }

    //memset(screen,color,SCREEN_SIZE);
    //memset(screen + SCREEN_SIZE + 16,color,SCREEN_SIZE);
}

void DrawCharacter(unsigned char *screen, int character, int x, int y, int color, int bgcolor)
{
    int yy, xx;
    for (yy = 0; yy < 8; yy++)
    {
        int xDisplacement = (x * BYTES_PER_PIXEL * SCREEN_WIDTH);
        int yDisplacement = ((SCREEN_WIDTH - (y + yy) - 1) * BYTES_PER_PIXEL);
        unsigned char *screenPos = screen + xDisplacement + yDisplacement;

        unsigned char charPos = font[character * 8 + yy];
        for (xx = 7; xx >= 0; xx--)
        {
            if ((charPos >> xx) & 1)
            {
                *(screenPos + 0) = color >> 16; //B
                *(screenPos + 1) = color >> 8; //G
                *(screenPos + 2) = color & 0xFF; //R
            }
            else
            {
                *(screenPos + 0) = bgcolor >> 16; //B
                *(screenPos + 1) = bgcolor >> 8; //G
                *(screenPos + 2) = bgcolor & 0xFF; //R
            }
            screenPos += BYTES_PER_PIXEL * SCREEN_WIDTH;
        }
    }
}

void DrawString(unsigned char *screen, const char *str, int x, int y, int color, int bgcolor)
{
    int i;
    for (i = 0; i < strlen(str); i++)
    {
        DrawCharacter(screen, str[i], x + i * 8, y, color, bgcolor);
    }
}

void DrawStringF(int x, int y, const char *format, ...)
{
    char* str;
    va_list va;

    va_start(va, format);
    vasprintf(&str, format, va);
    va_end(va);

    DrawString(TOP_SCREEN, str, x, y, RGB(0, 0, 0), RGB(255, 255, 255));
    free(str);
}

void DrawHex(unsigned char *screen, unsigned int hex, int x, int y, int color, int bgcolor)
{
    int i;
    for(i=0; i<8; i++)
    {
        int character = '-';
        int nibble = (hex >> ((7-i)*4))&0xF;
        if(nibble > 9) character = 'A' + nibble-10;
        else character = '0' + nibble;

        DrawCharacter(screen, character, x+(i*8), y, color, bgcolor);
    }
}

void DrawHexWithName(unsigned char *screen, const char *str, unsigned int hex, int x, int y, int color, int bgcolor)
{
    DrawString(screen, str, x, y, color, bgcolor);
    DrawHex(screen, hex,x + strlen(str) * 8, y, color, bgcolor);
}

void Debug(const char *format, ...)
{
    char* str;
    va_list va;

    va_start(va, format);
    vasprintf(&str, format, va);
    va_end(va);

    DrawString(TOP_SCREEN, str, 10, current_y, RGB(0, 0, 0), RGB(255, 255, 255));
    free(str);

    current_y += 10;
}
