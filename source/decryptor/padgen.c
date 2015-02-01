#include <string.h>

#include "fs.h"
#include "draw.h"
#include "decryptor/padgen.h"
#include "decryptor/crypto.h"

u32 ncchPadgen()
{
    size_t bytesRead;
    u32 result;

    struct ncch_info *info = (struct ncch_info *)0x20316000;

    u8 slot0x25KeyX[16] = {0};

    if (!FileOpen("/slot0x25KeyX.bin", false)) {
        Debug("Could not open slot0x25KeyX.bin!");
        return 1;
    }
    bytesRead = FileRead(&slot0x25KeyX, 16, 0);
    FileClose();
    if (bytesRead != 16) {
        Debug("slot0x25KeyX.bin is too small!");
        return 1;
    }
    setup_aeskeyX(0x25, slot0x25KeyX);

    Debug("Opening ncchinfo.bin ...");
    if (!FileOpen("/ncchinfo.bin", false)) {
        Debug("Could not open ncchinfo.bin!");
        return 1;
    }
    bytesRead = FileRead(info, 16, 0);

    if (!info->n_entries || info->n_entries > MAXENTRIES || (info->ncch_info_version != 0xF0000003)) {
        Debug("Too many/few entries, or wrong version ncchinfo.bin");
        return 0;
    }
    bytesRead = FileRead(info->entries, info->n_entries*sizeof(struct ncch_info_entry), 16);
    FileClose();

    Debug("Number of entries: %i", info->n_entries);

    for(u32 i = 0; i < info->n_entries; i++) {
        Debug("Creating pad number: %i  size (MB): %i", i+1, info->entries[i].size_mb);

        struct pad_info padInfo = {.setKeyY = 1, .size_mb = info->entries[i].size_mb};
        memcpy(padInfo.CTR, info->entries[i].CTR, 16);
        memcpy(padInfo.keyY, info->entries[i].keyY, 16);
        memcpy(padInfo.filename, info->entries[i].filename, 112);

        if(info->entries[i].uses7xCrypto)
            padInfo.keyslot = 0x25;
        else
            padInfo.keyslot = 0x2C;

        result = createPad(&padInfo);
        if (!result)
            Debug("Done!");
        else
            return 1;
    }

    return 0;
}

u32 sdPadgen()
{
    size_t bytesRead;
    u32 result;

    struct sd_info *info = (struct sd_info *)0x20316000;

    u8 movable_seed[0x120] = {0};

    // Load console 0x34 keyY from movable.sed if present on SD card
    if (FileOpen("/movable.sed", false)) {
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
    if (!FileOpen("/SDinfo.bin", false)) {
        Debug("Could not open SDinfo.bin!");
        return 1;
    }
    bytesRead = FileRead(info, 4, 0);

    if (!info->n_entries || info->n_entries > MAXENTRIES) {
        Debug("Too many/few entries!");
        return 1;
    }

    Debug("Number of entries: %i", info->n_entries);

    bytesRead = FileRead(info->entries, info->n_entries*sizeof(struct sd_info_entry), 4);
    FileClose();

    for(u32 i = 0; i < info->n_entries; i++) {
        Debug ("Creating pad number: %i size (MB): %i", i+1, info->entries[i].size_mb);

        struct pad_info padInfo = {.keyslot = 0x34, .setKeyY = 0, .size_mb = info->entries[i].size_mb};
        memcpy(padInfo.CTR, info->entries[i].CTR, 16);
        memcpy(padInfo.filename, info->entries[i].filename, 180);

        result = createPad(&padInfo);
        if (!result)
            Debug("Done!");
        else
            return 1;
    }

    return 0;
}

/*u32 nandPadgen()
{
    //Just trying to make sure the data we're looking for is where we think it is.
    //Should use some sort of memory scanning for it instead of hardcoding the address, though.
    if (*(u32*)0x080D7CAC != 0x5C980)
        return 1;

    u8 ctr[16] = {0x0};
    u32 i = 0;
    for(i = 0; i < 16; i++) {
        ctr[i] = *((u8*)(0x080D7CDC+(15-i))); //The CTR is stored backwards in memory.
    }
    add_ctr(ctr, 0xB93000); //The CTR stored in memory would theoretically be for NAND block 0, so we need to increment it some.

    Debug("Creating NAND FAT16 xorpad.  size (MB): 760");
    Debug("\tFilename: sdmc:/nand.fat16.xorpad");

    struct pad_info padInfo = {.keyslot = 0x4, .setKeyY = 0, .size_mb = 760, .filename = L"sdmc:/nand.fat16.xorpad"};
    //It's actually around 758MB in size. But meh, I'll just round up a bit.
    memcpy(padInfo.CTR, ctr, 16);

    u32 result = createPad(&padInfo);
    if(result == 0)
    {
        Debug("\tDone!");
        return 0;
    }
    else
    {
        return 1;
    }
}*/

inline u32 swap_uint32(u32 val)
{
    val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF );
    return (val << 16) | (val >> 16);
}

static const uint8_t zero_buf[16] __attribute__((aligned(16))) = {0};

u32 createPad(struct pad_info *info)
{
#define BUFFER_ADDR ((volatile uint8_t*)0x21000000)
#define BLOCK_SIZE  (1*1024*1024)
    size_t bytesWritten;

    if (!FileOpen(info->filename, true))
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

        bytesWritten = FileWrite((void*)BUFFER_ADDR, j, seekpos);
        seekpos += j;
        if(bytesWritten != j)
        {
            current_y = 50;
            Debug("ERROR, SD card may be full.");
            FileClose();
            return 1;
        }
    }

//    draw_fillrect(SCREEN_TOP_W-33, 1, 32, 8, BLACK);
    FileClose();
    return 0;
}
