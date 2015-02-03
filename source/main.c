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
    Debug("B: SD Padgen (untested)");
    Debug("X: Titlekey Decryption (untested)");
    while (true) {
        u32 pad_state = InputWait();
        if (pad_state & BUTTON_A) {
            ClearTop();
            Debug("NCCH Padgen: %s!", ncchPadgen() == 0 ? "succeeded" : "failed");
            break;
        } else if (pad_state & BUTTON_B) {
            ClearTop();
            Debug("SD Padgen: %s!", sdPadgen() == 0 ? "succeeded" : "failed");
            break;
        } else if (pad_state & BUTTON_X) {
            ClearTop();
            Debug("Titlekey Decryption: %s!", ncchPadgen() == 0 ? "succeeded" : "failed");
            break;
        }
    }

    DeinitFS();
    return 0;
}
