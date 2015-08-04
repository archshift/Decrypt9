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
        "XORpad Options",
        {
            { "NCCH Padgen", &NcchPadgen },
            { "SD Padgen", &SdPadgen },
            { "CTRNAND Padgen", &CtrNandPadgen },
            { "TWLNAND Padgen", &TwlNandPadgen }
        }
    },
    {
        "Other Options",
        {
            { "NAND Backup", &DumpNand },
            { "TWL / AGB Partitions Dump", &DecryptTwlAgbPartitions },
            { "CTR Partitions Dump", &DecryptCtrPartitions },
            { "Titlekey Decrypt", &DecryptTitlekeys }
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
