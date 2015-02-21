#include <string.h>

#include "fs.h"
#include "draw.h"
#include "decryptor/padgen.h"
#include "decryptor/crypto.h"

u32 NcchPadgen()
{
    size_t bytesRead;
    u32 result;

    NcchInfo *info = (NcchInfo*)0x20316000;

    if (FileOpen("/slot0x25KeyX.bin")) {
        u8 slot0x25KeyX[16] = {0};
        Debug("Opening slot0x25KeyX.bin ...");
        
        bytesRead = FileRead(&slot0x25KeyX, 16, 0);
        FileClose();
        if (bytesRead != 16) {
            Debug("slot0x25KeyX.bin is too small!");
            return 1;
        }
        setup_aeskeyX(0x25, slot0x25KeyX);
    } else {
        Debug("Warning, not using slot0x25KeyX.bin");
        Debug("7.x game decryption will fail on less than 7.x!");
    }

    Debug("Opening ncchinfo.bin ...");
    if (!FileOpen("/ncchinfo.bin")) {
        Debug("Could not open ncchinfo.bin!");
        return 1;
    }
    bytesRead = FileRead(info, 16, 0);

    if (!info->n_entries || info->n_entries > MAXENTRIES || (info->ncch_info_version != 0xF0000003)) {
        Debug("Too many/few entries, or wrong version ncchinfo.bin");
        return 0;
    }
    bytesRead = FileRead(info->entries, info->n_entries * sizeof(NcchInfoEntry), 16);
    FileClose();

    Debug("Number of entries: %i", info->n_entries);

    for(u32 i = 0; i < info->n_entries; i++) {
        Debug("Creating pad number: %i  size (MB): %i", i+1, info->entries[i].size_mb);

        PadInfo padInfo = {.setKeyY = 1, .size_mb = info->entries[i].size_mb};
        memcpy(padInfo.CTR, info->entries[i].CTR, 16);
        memcpy(padInfo.keyY, info->entries[i].keyY, 16);
        memcpy(padInfo.filename, info->entries[i].filename, 112);

        if(info->entries[i].uses7xCrypto)
            padInfo.keyslot = 0x25;
        else
            padInfo.keyslot = 0x2C;

        result = CreatePad(&padInfo);
        if (!result)
            Debug("Done!");
        else
            return 1;
    }

    return 0;
}

u32 SdPadgen()
{
    size_t bytesRead;
    u32 result;

    SdInfo *info = (SdInfo*)0x20316000;

    u8 movable_seed[0x120] = {0};

    // Load console 0x34 keyY from movable.sed if present on SD card
    if (FileOpen("/movable.sed")) {
        Debug("Loading custom movable.sed");
        bytesRead = FileRead(&movable_seed, 0x120, 0);
        FileClose();
        if (bytesRead != 0x120) {
            Debug("movable.sed is too small!");
            return 1;
        }
        if (memcmp(movable_seed, "SEED", 4) != 0) {
            Debug("movable.sed is too corrupt!");
            return 1;
        }

        setup_aeskey(0x34, AES_BIG_INPUT|AES_NORMAL_INPUT, &movable_seed[0x110]);
        use_aeskey(0x34);
    }

    Debug("Opening SDinfo.bin ...");
    if (!FileOpen("/SDinfo.bin")) {
        Debug("Could not open SDinfo.bin!");
        return 1;
    }
    bytesRead = FileRead(info, 4, 0);

    if (!info->n_entries || info->n_entries > MAXENTRIES) {
        Debug("Too many/few entries!");
        return 1;
    }

    Debug("Number of entries: %i", info->n_entries);

    bytesRead = FileRead(info->entries, info->n_entries * sizeof(SdInfoEntry), 4);
    FileClose();

    for(u32 i = 0; i < info->n_entries; i++) {
        Debug ("Creating pad number: %i size (MB): %i", i+1, info->entries[i].size_mb);

        PadInfo padInfo = {.keyslot = 0x34, .setKeyY = 0, .size_mb = info->entries[i].size_mb};
        memcpy(padInfo.CTR, info->entries[i].CTR, 16);
        memcpy(padInfo.filename, info->entries[i].filename, 180);

        result = CreatePad(&padInfo);
        if (!result)
            Debug("Done!");
        else
            return 1;
    }

    return 0;
}

u32 NandPadgen()
{
    //Just trying to make sure the data we're looking for is where we think it is.
    //Should use some sort of memory scanning for it instead of hardcoding the address, though.
    if (*(u32*)0x080D794C != 0x5C980)
        return 1;

    u8 ctr[16] = {0x0};
    u32 i = 0;
    for(i = 0; i < 16; i++) {
        ctr[i] = *((u8*)(0x080D797C+(15-i))); //The CTR is stored backwards in memory.
    }
    add_ctr(ctr, 0xB93000); //The CTR stored in memory would theoretically be for NAND block 0, so we need to increment it some.

    Debug("Creating NAND FAT16 xorpad.  size (MB): 760");
    Debug("Filename: sdmc:/nand.fat16.xorpad");

    PadInfo padInfo = {.keyslot = 0x4, .setKeyY = 0, .size_mb = 760, .filename = "/nand.fat16.xorpad"};
    //It's actually around 758MB in size. But meh, I'll just round up a bit.
    memcpy(padInfo.CTR, ctr, 16);

    u32 result = CreatePad(&padInfo);
    if(result == 0) {
        Debug("Done!");
        return 0;
    } else {
        return 1;
    }
}

static const uint8_t zero_buf[16] __attribute__((aligned(16))) = {0};

u32 CreatePad(PadInfo *info)
{
#define BUFFER_ADDR ((volatile uint8_t*)0x21000000)
#define BLOCK_SIZE  (4*1024*1024)
    size_t bytesWritten;

    if (!FileCreate(info->filename, true))
        return 1;

    if(info->setKeyY)
        setup_aeskey(info->keyslot, AES_BIG_INPUT|AES_NORMAL_INPUT, info->keyY);
    use_aeskey(info->keyslot);

    u8 ctr[16] __attribute__((aligned(32)));
    memcpy(ctr, info->CTR, 16);

    u32 size_bytes = info->size_mb*1024*1024;
    u32 size_100 = size_bytes/100;
    u32 seekpos = 0;
    for (u32 i = 0; i < size_bytes; i += BLOCK_SIZE) {
        u32 j;
        for (j = 0; (j < BLOCK_SIZE) && (i+j < size_bytes); j+= 16) {
            set_ctr(AES_BIG_INPUT|AES_NORMAL_INPUT, ctr);
            aes_decrypt((void*)zero_buf, (void*)BUFFER_ADDR+j, ctr, 1, AES_CTR_MODE);
            add_ctr(ctr, 1);
        }

        DrawStringF(SCREEN_HEIGHT-43, 1, "%*i%%", 2, (i+j)/size_100);
        bytesWritten = FileWrite((void*)BUFFER_ADDR, j, seekpos);
        seekpos += j;
        if(bytesWritten != j)
        {
            Debug("ERROR, SD card may be full.");
            FileClose();
            return 1;
        }
    }

    DrawStringF(SCREEN_HEIGHT-43, 1, "    ");
    FileClose();
    return 0;
}
