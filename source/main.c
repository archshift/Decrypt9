#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"

#include "fs.h"
#include "draw.h"

int main()
{
	ClearScreen(TOP_SCREEN, RGB(0, 255, 0));

    InitFS();

    Debug("Padgen: %s", ncchPadgen() == 0 ? "succeeded" : "failed");
	return 0;
}