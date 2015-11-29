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
        "XORpad Options", 4,
        {
            { "NCCH Padgen", &NcchPadgen, false, false, 0 },
            { "SD Padgen", &SdPadgen, false, false, 0 },
            { "CTRNAND Padgen", &CtrNandPadgen, false, false, 0 },
            { "TWLNAND Padgen", &TwlNandPadgen, false, false, 0 }
        }
    },
    {
        "NAND Options 1", 4,
        {
            { "NAND Backup", &DumpNand, false, false, 0 },
            { "All Partitions Dump", &DecryptAllNandPartitions, false, false, 0 },
            { "TWLNAND Partition Dump", &DecryptTwlNandPartition, false, false, 0 },
            { "CTRNAND Partition Dump", &DecryptCtrNandPartition, false, false, 0 }
        }
    },
    {
        "NAND Options 2", 4,
        {
            { "NAND Restore", &RestoreNand, true, false, 0 },
            { "All Partitions Inject", &InjectAllNandPartitions, true, false, 0 },
            { "TWLNAND Partition Inject", &InjectTwlNandPartition, true, false, 0 },
            { "CTRNAND Partition Inject", &InjectCtrNandPartition, true, false, 0 }
        }
    },
    {
        "Titles Options", 4,
        {
            { "Titlekey Decrypt (file)", &DecryptTitlekeysFile, false, false, 0 },
            { "Titlekey Decrypt (NAND)", &DecryptTitlekeysNand, false, false, 0 },
            { "Ticket Dump", &DumpTicket, false, false, 0 },
            { "NCCH Decryptor", &DecryptNcsdNcchBatch, false, false, 0 }
        }
    },
    {
        "EmuNAND Options", 4,
        {
            { "Titlekey Decrypt", &DecryptTitlekeysNand, false, true, 0 },
            { "Ticket Dump", &DumpTicket, false, true, 0 },
            { "Update SeedDB", &UpdateSeedDb, false, true, 0 },
            { "Seedsave Dump", &DumpSeedsave, false, true, 0 }
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
    ClearScreenFull(true, true);
    InitFS();

    ProcessMenu(menu, sizeof(menu) / sizeof(MenuInfo));
    
    DeinitFS();
    Reboot();
    return 0;
}
