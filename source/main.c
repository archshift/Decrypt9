#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "draw.h"
#include "fs.h"
#include "hid.h"
#include "menu.h"
#include "i2c.h"
#include "decryptor/features.h"


MenuInfo menu[] =
{
    {
        "Decrypt9 Features #1",
        {
            { "NCCH Padgen", &NcchPadgen },
            { "SD Padgen", &SdPadgen },
            { "CTRNAND Padgen", &CtrNandPadgen },
            { "Titlekey Decrypt", &DecryptTitlekeys }
        }
    },
    {
        "Decrypt9 Features #2",
        {
            { "NAND Backup", &DumpNand },
            { "NAND Partitions Dump", &DecryptCtrPartitions },
            { NULL, NULL },
            { NULL, NULL },
        }
    }
};
        

void Reboot()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(true);
}

int main()
{
    DebugClear();
    InitFS();

    ProcessMenu(menu, sizeof(menu) / sizeof(MenuInfo));
    
    DeinitFS();
    Reboot();
    return 0;
}
