#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "draw.h"
#include "fs.h"
#include "hid.h"
#include "decryptor/padgen.h"

void ClearTop()
{
    ClearScreen(TOP_SCREEN, RGB(255, 255, 255));
    current_y = 0;
}

int main()
{
    ClearTop();
    InitFS();

    Debug("A: NCCH Padgen");
    while (true) {
        u32 pad_state = InputWait();
        if (pad_state & BUTTON_A) {
            ClearTop();
            Debug("Padgen: %s", ncchPadgen() == 0 ? "succeeded" : "failed");
            break;
        }
    }

    DeinitFS();
    return 0;
}
