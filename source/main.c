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

#define SUBMENU_START 5


MenuInfo menu[] =
{
    {
        "XORpad Generator Options", 4,
        {
            { "NCCH Padgen",                  &NcchPadgen,            false, false, 0 },
            { "SD Padgen",                    &SdPadgen,              false, false, 0 },
            { "CTRNAND Padgen",               &CtrNandPadgen,         false, false, 0 },
            { "TWLNAND Padgen",               &TwlNandPadgen,         false, false, 0 }
        }
    },
    {
        "Titlekey Decrypt Options", 3,
        {
            { "Titlekey Decrypt (file)",      &DecryptTitlekeysFile,  false, false, 0 },
            { "Titlekey Decrypt (SysNAND)",   &DecryptTitlekeysNand,  false, false, 0 },
            { "Titlekey Decrypt (EmuNAND)",   &DecryptTitlekeysNand,  false, true,  0 }
        }
    },
    {
        "SysNAND Options", 6,
        {
            { "NAND Backup",                  &DumpNand,              false, false, 0 },
            { "NAND Restore",                 &RestoreNand,           true,  false, 0 },
            { "Partition Dump...",            NULL,                   false, false, SUBMENU_START + 0 },
            { "Partition Inject...",          NULL,                   true,  false, SUBMENU_START + 2 },
            { "File Dump...",                 NULL,                   false, false, SUBMENU_START + 4 },
            { "File Inject...",               NULL,                   true,  false, SUBMENU_START + 6 }
        }
    },
    {
        "EmuNAND Options", 7,
        {
            { "EmuNAND Backup",               &DumpNand,              false, true,  0 },
            { "EmuNAND Restore",              &RestoreNand,           true,  true,  0 },
            { "Partition Dump...",            NULL,                   false, true,  SUBMENU_START + 1 },
            { "Partition Inject...",          NULL,                   true,  true,  SUBMENU_START + 3 },
            { "File Dump...",                 NULL,                   false, true,  SUBMENU_START + 5 },
            { "File Inject...",               NULL,                   true,  true,  SUBMENU_START + 7 },
            { "Update SeedDB",                &UpdateSeedDb,          false, true,  0 }
        }
    },
    {
        "Game Decryptor Options", 1,
        {
            { "NCCH/NCSD Decryptor",          &DecryptNcsdNcchBatch,  false, false, 0 }
        }
    },
    // everything below is not contained in the main menu
    {
        "Partition Dump... (SysNAND)", 6, // ID 0
        {
            { "Dump TWLN Partition",          &DecryptNandPartitions, false, false, P_TWLN },
            { "Dump TWLP Partition",          &DecryptNandPartitions, false, false, P_TWLP },
            { "Dump AGBSAVE Partition",       &DecryptNandPartitions, false, false, P_AGBSAVE },
            { "Dump FIRM0 Partition",         &DecryptNandPartitions, false, false, P_FIRM0 },
            { "Dump FIRM1 Partition",         &DecryptNandPartitions, false, false, P_FIRM1 },
            { "Dump CTRNAND Partition",       &DecryptNandPartitions, false, false, P_CTRNAND }
        }
    },
    {
        "Partition Dump...(EmuNAND)", 6, // ID 1
        {
            { "Dump TWLN Partition",          &DecryptNandPartitions, false, true,  P_TWLN },
            { "Dump TWLP Partition",          &DecryptNandPartitions, false, true,  P_TWLP },
            { "Dump AGBSAVE Partition",       &DecryptNandPartitions, false, true,  P_AGBSAVE },
            { "Dump FIRM0 Partition",         &DecryptNandPartitions, false, true,  P_FIRM0 },
            { "Dump FIRM1 Partition",         &DecryptNandPartitions, false, true,  P_FIRM1 },
            { "Dump CTRNAND Partition",       &DecryptNandPartitions, false, true,  P_CTRNAND }
        }
    },
    {
        "Partition Inject... (SysNAND)", 6, // ID 2
        {
            { "Inject TWLN Partition",        &InjectNandPartitions, true,  false, P_TWLN },
            { "Inject TWLP Partition",        &InjectNandPartitions, true,  false, P_TWLP },
            { "Inject AGBSAVE Partition",     &InjectNandPartitions, true,  false, P_AGBSAVE },
            { "Inject FIRM0 Partition",       &InjectNandPartitions, true,  false, P_FIRM0 },
            { "Inject FIRM1 Partition",       &InjectNandPartitions, true,  false, P_FIRM1 },
            { "Inject CTRNAND Partition",     &InjectNandPartitions, true,  false, P_CTRNAND }
        }
    },
    {
        "Partition Inject... (EmuNAND)", 6, // ID 3
        {
            { "Inject TWLN Partition",        &InjectNandPartitions, true,  true,  P_TWLN },
            { "Inject TWLP Partition",        &InjectNandPartitions, true,  true,  P_TWLP },
            { "Inject AGBSAVE Partition",     &InjectNandPartitions, true,  true,  P_AGBSAVE },
            { "Inject FIRM0 Partition",       &InjectNandPartitions, true,  true,  P_FIRM0 },
            { "Inject FIRM1 Partition",       &InjectNandPartitions, true,  true,  P_FIRM1 },
            { "Inject CTRNAND Partition",     &InjectNandPartitions, true,  true,  P_CTRNAND }
        }
    },
    {
        "File Dump... (SysNAND)", 8, // ID 4
        {
            { "Dump ticket.db",               &DumpFile,             false, false, F_TICKET },
            { "Dump title.db",                &DumpFile,             false, false, F_TITLE },
            { "Dump import.db",               &DumpFile,             false, false, F_IMPORT },
            { "Dump certs.db",                &DumpFile,             false, false, F_CERTS },
            { "Dump SecureInfo_A",            &DumpFile,             false, false, F_SECUREINFO },
            { "Dump LocalFriendCodeSeed_B",   &DumpFile,             false, false, F_LOCALFRIEND },
            { "Dump rand_seed",               &DumpFile,             false, false, F_RANDSEED },
            { "Dump movable.sed",             &DumpFile,             false, false, F_MOVABLE }
        }
    },
    {
        "File Dump... (EmuNAND)", 9, // ID 5
        {
            { "Dump ticket.db",               &DumpFile,             false, true,  F_TICKET },
            { "Dump title.db",                &DumpFile,             false, true,  F_TITLE },
            { "Dump import.db",               &DumpFile,             false, true,  F_IMPORT },
            { "Dump certs.db",                &DumpFile,             false, true,  F_CERTS },
            { "Dump SecureInfo_A",            &DumpFile,             false, true,  F_SECUREINFO },
            { "Dump LocalFriendCodeSeed_B",   &DumpFile,             false, true,  F_LOCALFRIEND },
            { "Dump rand_seed",               &DumpFile,             false, true,  F_RANDSEED },
            { "Dump movable.sed",             &DumpFile,             false, true,  F_MOVABLE },
            { "Dump seedsave.bin",            &DumpFile,             false, true,  F_SEEDSAVE }
        }
    },
    {
        "File Inject... (SysNAND)", 8, // ID 6
        {
            { "Inject ticket.db",             &InjectFile,           true,  false, F_TICKET },
            { "Inject title.db",              &InjectFile,           true,  false, F_TITLE },
            { "Inject import.db",             &InjectFile,           true,  false, F_IMPORT },
            { "Inject certs.db",              &InjectFile,           true,  false, F_CERTS },
            { "Inject SecureInfo_A",          &InjectFile,           true,  false, F_SECUREINFO },
            { "Inject LocalFriendCodeSeed_B", &InjectFile,           true,  false, F_LOCALFRIEND },
            { "Inject rand_seed",             &InjectFile,           true,  false, F_RANDSEED },
            { "Inject movable.sed",           &InjectFile,           true,  false, F_MOVABLE }
        }
    },
    {
        "File Inject... (EmuNAND)", 9, // ID 7
        {
            { "Inject ticket.db",             &InjectFile,           true,  true,  F_TICKET },
            { "Inject title.db",              &InjectFile,           true,  true,  F_TITLE },
            { "Inject import.db",             &InjectFile,           true,  true,  F_IMPORT },
            { "Inject certs.db",              &InjectFile,           true,  true,  F_CERTS },
            { "Inject SecureInfo_A",          &InjectFile,           true,  true,  F_SECUREINFO },
            { "Inject LocalFriendCodeSeed_B", &InjectFile,           true,  true,  F_LOCALFRIEND },
            { "Inject rand_seed",             &InjectFile,           true,  true,  F_RANDSEED },
            { "Inject movable.sed",           &InjectFile,           true,  true,  F_MOVABLE },
            { "Inject seedsave.bin",          &InjectFile,           true,  true,  F_SEEDSAVE }
        }
    },
    {
        NULL, 0, {}, // empty menu to signal end
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

    ProcessMenu(menu, SUBMENU_START);
    
    DeinitFS();
    Reboot();
    return 0;
}
