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
            { "NCCH Padgen", &NcchPadgen, 0 },
            { "SD Padgen", &SdPadgen, 0 },
            { "CTRNAND Padgen", &CtrNandPadgen, 0 },
            { "TWLNAND Padgen", &TwlNandPadgen, 0 }
        }
    },
    {
        "NAND Options 1",
        {
            { "NAND Backup", &DumpNand, 0 },
            { "All Partitions Dump", &DecryptAllNandPartitions, 0 },
            { "TWLNAND Partition Dump", &DecryptTwlNandPartition, 0 },
            { "CTRNAND Partition Dump", &DecryptCtrNandPartition, 0 }
        }
    },
    {
        "NAND Options 2",
        {
            { "NAND Restore", &RestoreNand, 1 },
            { "All Partitions Inject", &InjectAllNandPartitions, 1 },
            { "TWLNAND Partition Inject", &InjectTwlNandPartition, 1 },
            { "CTRNAND Partition Inject", &InjectCtrNandPartition, 1 }
        }
    },
    {
        "Titlekey Options",
        {
            { "Titlekey Decrypt (file)", &DecryptTitlekeysFile, 0 },
            { "Titlekey Decrypt (NAND)", &DecryptTitlekeysNand, 0 },
            { "Ticket Dump", &DumpTicket, 0 },
            { NULL, NULL }
        }
    },
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
