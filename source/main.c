#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "draw.h"
#include "fs.h"
#include "decryptor/padgen.h"

int main()
{
	ClearScreen(TOP_SCREEN, RGB(0, 255, 0));

    InitFS();

    Debug("Padgen: %s", ncchPadgen() == 0 ? "succeeded" : "failed");

    DeinitFS();
	return 0;
}
