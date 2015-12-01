#include "common.h"
#include "draw.h"
#include "fs.h"
#include "hid.h"
#include "menu.h"
#include "i2c.h"
#include "decryptor/game.h"
#include "decryptor/nand.h"
#include "decryptor/nandfat.h"
#include "decryptor/titlekey.h"


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
            { "All Partitions Dump", &DecryptNandPartitions, false, false, P_ALL },
            { "TWLNAND Partition Dump", &DecryptNandPartitions, false, false, P_TWLN },
            { "CTRNAND Partition Dump", &DecryptNandPartitions, false, false, P_CTRNAND }
        }
    },
    {
        "NAND Options 2", 4,
        {
            { "NAND Restore", &RestoreNand, true, false, 0 },
            { "All Partitions Inject", &InjectNandPartitions, true, false, P_ALL },
            { "TWLNAND Partition Inject", &InjectNandPartitions, true, false, P_TWLN },
            { "CTRNAND Partition Inject", &InjectNandPartitions, true, false, P_CTRNAND }
        }
    },
    {
        "Titles Options", 4,
        {
            { "Titlekey Decrypt (file)", &DecryptTitlekeysFile, false, false, 0 },
            { "Titlekey Decrypt (NAND)", &DecryptTitlekeysNand, false, false, 0 },
            { "Ticket Dump", &DumpFile, false, false, F_TICKET },
            { "NCCH Decryptor", &DecryptNcsdNcchBatch, false, false, 0 }
        }
    },
    {
        "EmuNAND Options", 4,
        {
            { "Titlekey Decrypt", &DecryptTitlekeysNand, false, true, 0 },
            { "Ticket Dump", &DumpFile, false, true, F_TICKET },
            { "Update SeedDB", &UpdateSeedDb, false, true, 0 },
            { "Seedsave Dump", &DumpFile, false, true, F_SEEDSAVE }
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
