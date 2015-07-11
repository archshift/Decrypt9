#include <string.h>

#include "fs.h"
#include "draw.h"
#include "platform.h"
#include "decryptor/padgen.h"
#include "decryptor/crypto.h"
#include "sha256.h"

#define BUFFER_ADDRESS  ((u8*) 0x21000000)
#define BUFFER_MAX_SIZE (1 * 1024 * 1024)

u32 NcchPadgen()
{
    size_t bytesRead;
    u32 result;

    NcchInfo *info = (NcchInfo*)0x20316000;
    SeedInfo *seedinfo = (SeedInfo*)0x20400000;

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

    if (FileOpen("/seeddb.bin")) {
        Debug("Opening seeddb.bin ...");
        bytesRead = FileRead(seedinfo, 16, 0);
        if (!seedinfo->n_entries || seedinfo->n_entries > MAXENTRIES) {
            Debug("Too many/few seeddb entries.");
            return 0;
        }
        bytesRead = FileRead(seedinfo->entries, seedinfo->n_entries * sizeof(SeedInfoEntry), 16);
        FileClose();
    } else {
        Debug("Warning, didn't open seeddb.bin");
        Debug("9.x seed crypto game decryption will fail!");
    }

    Debug("Opening ncchinfo.bin ...");
    if (!FileOpen("/ncchinfo.bin")) {
        Debug("Could not open ncchinfo.bin!");
        return 1;
    }
    bytesRead = FileRead(info, 16, 0);

    if (!info->n_entries || info->n_entries > MAXENTRIES || (info->ncch_info_version != 0xF0000004)) {
        Debug("Too many/few entries, or wrong version ncchinfo.bin");
        return 0;
    }
    bytesRead = FileRead(info->entries, info->n_entries * sizeof(NcchInfoEntry), 16);
    FileClose();

    Debug("Number of entries: %i", info->n_entries);

    for(u32 i = 0; i < info->n_entries; i++) {
        Debug("Creating pad number: %i. Size (MB): %i", i+1, info->entries[i].size_mb);

        PadInfo padInfo = {.setKeyY = 1, .size_mb = info->entries[i].size_mb};
        memcpy(padInfo.CTR, info->entries[i].CTR, 16);
        memcpy(padInfo.filename, info->entries[i].filename, 112);
        if (info->entries[i].uses7xCrypto && info->entries[i].usesSeedCrypto) {
            u8 keydata[32];
            memcpy(keydata, info->entries[i].keyY, 16);
            u32 found_seed = 0;
            for (u32 j = 0; j < seedinfo->n_entries; j++) {
                if (seedinfo->entries[j].titleId == info->entries[i].titleId) {
                    found_seed = 1;
                    memcpy(&keydata[16], seedinfo->entries[j].external_seed, 16);
                    break;
                }
            }
            if (!found_seed)
            {
                Debug("Failed to find seed in seeddb.bin");
                return 0;
            }
        u8 sha256sum[32];
            sha256_context shactx;
            sha256_starts(&shactx);
            sha256_update(&shactx, keydata, 32);
            sha256_finish(&shactx, sha256sum);
            memcpy(padInfo.keyY, sha256sum, 16);
        }
        else
            memcpy(padInfo.keyY, info->entries[i].keyY, 16);

        if(info->entries[i].uses7xCrypto == 0xA) // won't work on an Old 3DS
            padInfo.keyslot = 0x18;
        else if(info->entries[i].uses7xCrypto)
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
        Debug ("Creating pad number: %i. Size (MB): %i", i+1, info->entries[i].size_mb);

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

static u8* FindNandCtr()
{
    static const char* versions[] = {"4.x", "5.x", "6.x", "7.x", "8.x", "9.x"};
    static const u8* version_ctrs[] = {
        (u8*)0x080D7CAC,
        (u8*)0x080D858C,
        (u8*)0x080D748C,
        (u8*)0x080D740C,
        (u8*)0x080D74CC,
        (u8*)0x080D794C
    };
    static const u32 version_ctrs_len = sizeof(version_ctrs) / sizeof(u32);

    for (u32 i = 0; i < version_ctrs_len; i++) {
        if (*(u32*)version_ctrs[i] == 0x5C980) {
            Debug("System version %s", versions[i]);
            return (u8*)(version_ctrs[i] + 0x30);
        }
    }

    // If value not in previous list start memory scanning (test range)
    for (u8* c = (u8*)0x080D8FFF; c > (u8*)0x08000000; c--) {
        if (*(u32*)c == 0x5C980 && *(u32*)(c + 1) == 0x800005C9) {
            Debug("CTR Start 0x%08X", c + 0x30);
            return c + 0x30;
        }
    }

    return NULL;
}

u32 NandPadgen()
{
    u8* ctrStart = FindNandCtr();
    if (ctrStart == NULL)
        return 1;

    u8 ctr[16] = {0x0};
    u32 i = 0;
    for(i = 0; i < 16; i++)
        ctr[i] = *(ctrStart + (15 - i)); //The CTR is stored backwards in memory.

    add_ctr(ctr, 0xB93000); //The CTR stored in memory would theoretically be for NAND block 0, so we need to increment it some.

    u32 keyslot = 0x0;
    u32 nand_size = 0;
    switch (GetUnitPlatform()) {
        case PLATFORM_3DS:
            keyslot = 0x4;
            nand_size = 758;
            break;
        case PLATFORM_N3DS:
            keyslot = 0x5;
            nand_size = 1055;
            break;
    }

    Debug("Creating NAND FAT16 xorpad. Size (MB): %u", nand_size);
    Debug("Filename: nand.fat16.xorpad");

    PadInfo padInfo = {.keyslot = keyslot, .setKeyY = 0, .size_mb = nand_size , .filename = "/nand.fat16.xorpad"};
    memcpy(padInfo.CTR, ctr, 16);

    u32 result = CreatePad(&padInfo);
    if(result == 0) {
        Debug("Done!");
        return 0;
    } else {
        return 1;
    }
}

u32 CreatePad(PadInfo *info)
{
    static const uint8_t zero_buf[16] __attribute__((aligned(16))) = {0};

    u8* buffer = BUFFER_ADDRESS;
    size_t bytesWritten;

    if (!FileCreate(info->filename, true))
        return 1;

    if(info->setKeyY)
        setup_aeskey(info->keyslot, AES_BIG_INPUT | AES_NORMAL_INPUT, info->keyY);
    use_aeskey(info->keyslot);

    u8 ctr[16] __attribute__((aligned(32)));
    memcpy(ctr, info->CTR, 16);

    u32 size_bytes = info->size_mb * 1024*1024;
    for (u32 i = 0; i < size_bytes; i += BUFFER_MAX_SIZE) {
        u32 curr_block_size = min(BUFFER_MAX_SIZE, size_bytes - i);

        for (u32 j = 0; j < curr_block_size; j+= 16) {
            set_ctr(AES_BIG_INPUT | AES_NORMAL_INPUT, ctr);
            aes_decrypt((void*)zero_buf, (void*)buffer + j, ctr, 1, AES_CTR_MODE);
            add_ctr(ctr, 1);
        }

        ShowProgress(i, size_bytes);

        bytesWritten = FileWrite((void*)buffer, curr_block_size, i);
        if (bytesWritten != curr_block_size) {
            Debug("ERROR, SD card may be full.");
            FileClose();
            return 1;
        }
    }

    ShowProgress(0, 0);
    FileClose();

    return 0;
}
