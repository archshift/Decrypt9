#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "draw.h"
#include "fs.h"
#include "hid.h"
#include "decryptor/padgen.h"
#include "decryptor/titlekey.h"

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
    Debug("B: SD Padgen");
    Debug("X: Titlekey Decryption");
    Debug("Y: NAND Padgen (untested in 5.x, 8.x)");
    while (true) {
        u32 pad_state = InputWait();
        if (pad_state & BUTTON_A) {
            ClearTop();
            Debug("NCCH Padgen: %s!", NcchPadgen() == 0 ? "succeeded" : "failed");
            break;
        } else if (pad_state & BUTTON_B) {
            ClearTop();
            Debug("SD Padgen: %s!", SdPadgen() == 0 ? "succeeded" : "failed");
            break;
        } else if (pad_state & BUTTON_X) {
            ClearTop();
            Debug("Titlekey Decryption: %s!", DecryptTitlekeys() == 0 ? "succeeded" : "failed");
            break;
        } else if (pad_state & BUTTON_Y) {
            ClearTop();
            Debug("NAND Padgen: %s!", NandPadgen() == 0 ? "succeeded" : "failed");
            break;
        }
    }

    DeinitFS();
    return 0;
}
