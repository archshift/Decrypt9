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
            { "NCCH Padgen", &NcchPadgen, false, false },
            { "SD Padgen", &SdPadgen, false, false },
            { "CTRNAND Padgen", &CtrNandPadgen, false, false },
            { "TWLNAND Padgen", &TwlNandPadgen, false, false }
        }
    },
    {
        "NAND Options 1",
        {
            { "NAND Backup", &DumpNand, false, false },
            { "All Partitions Dump", &DecryptAllNandPartitions, false, false },
            { "TWLNAND Partition Dump", &DecryptTwlNandPartition, false, false },
            { "CTRNAND Partition Dump", &DecryptCtrNandPartition, false, false }
        }
    },
    {
        "NAND Options 2",
        {
            { "NAND Restore", &RestoreNand, true, false },
            { "All Partitions Inject", &InjectAllNandPartitions, true, false },
            { "TWLNAND Partition Inject", &InjectTwlNandPartition, true, false },
            { "CTRNAND Partition Inject", &InjectCtrNandPartition, true, false }
        }
    },
    {
        "Titles Options",
        {
            { "Titlekey Decrypt (file)", &DecryptTitlekeysFile, false, false },
            { "Titlekey Decrypt (NAND)", &DecryptTitlekeysNand, false, false },
            { "Ticket Dump", &DumpTicket, false, false },
            { "NCCH Decryptor", &DecryptNcsdNcchBatch, false, false }
        }
    },
    {
        "EmuNAND Options",
        {
            { "Titlekey Decrypt", &DecryptTitlekeysNand, false, true },
            { "Ticket Dump", &DumpTicket, false, true },
            { "Update SeedDB", &UpdateSeedDb, false, true },
            { "Seedsave Dump", &DumpSeedsave, false, true }
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
