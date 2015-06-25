#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "draw.h"
#include "fs.h"
#include "hid.h"
#include "i2c.h"
#include "decryptor/features.h"


void Reboot()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(true);
}

int main()
{
    DebugClear();
    InitFS();

    Debug("A: NCCH Padgen");
    Debug("B: SD Padgen");
    Debug("X: Titlekey Decryption");
    Debug("Y: NAND Padgen");
	Debug("R: NAND Dump");
    Debug("");
    Debug("START: Reboot");
    Debug("");
    Debug("");
    Debug("Remaining SD storage space: %llu MiB", RemainingStorageSpace() / 1024 / 1024);

    while (true) {
        u32 pad_state = InputWait();
        if (pad_state & BUTTON_A) {
			DebugClear();
            Debug("NCCH Padgen: %s!", NcchPadgen() == 0 ? "succeeded" : "failed");
            break;
        } else if (pad_state & BUTTON_B) {
			DebugClear();
            Debug("SD Padgen: %s!", SdPadgen() == 0 ? "succeeded" : "failed");
            break;
        } else if (pad_state & BUTTON_X) {
			DebugClear();
            Debug("Titlekey Decryption: %s!", DecryptTitlekeys() == 0 ? "succeeded" : "failed");
            break;
        } else if (pad_state & BUTTON_Y) {
			DebugClear();
            Debug("NAND Padgen: %s!", NandPadgen() == 0 ? "succeeded" : "failed");
            break;
        } else if (pad_state & BUTTON_R1) {
			DebugClear();
			Debug("NAND Dump: %s!", NandDumper() == 0 ? "succeeded" : "failed");
            break;
        } else if (pad_state & BUTTON_START) {
            DeinitFS();
            Reboot();
            return 0;
        }
    }

    Debug("");
    Debug("Press START to reboot to home.");
    while(true) {
        if (InputWait() & BUTTON_START)
            break;
    }

    DeinitFS();
    Reboot();
    return 0;
}
