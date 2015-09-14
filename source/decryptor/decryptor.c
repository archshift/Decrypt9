#include <string.h>
#include <stdio.h>

#include "fs.h"
#include "draw.h"
#include "platform.h"
#include "decryptor/decryptor.h"
#include "decryptor/crypto.h"
#include "decryptor/features.h"
#include "sha256.h"

#define BUFFER_ADDRESS  ((u8*) 0x21000000)
#define BUFFER_MAX_SIZE (1 * 1024 * 1024)

#define NAND_SECTOR_SIZE 0x200
#define SECTORS_PER_READ (BUFFER_MAX_SIZE / NAND_SECTOR_SIZE)

// From https://github.com/profi200/Project_CTR/blob/master/makerom/pki/prod.h#L19
static const u8 common_keyy[6][16] = {
    {0xD0, 0x7B, 0x33, 0x7F, 0x9C, 0xA4, 0x38, 0x59, 0x32, 0xA2, 0xE2, 0x57, 0x23, 0x23, 0x2E, 0xB9} , // 0 - eShop Titles
    {0x0C, 0x76, 0x72, 0x30, 0xF0, 0x99, 0x8F, 0x1C, 0x46, 0x82, 0x82, 0x02, 0xFA, 0xAC, 0xBE, 0x4C} , // 1 - System Titles
    {0xC4, 0x75, 0xCB, 0x3A, 0xB8, 0xC7, 0x88, 0xBB, 0x57, 0x5E, 0x12, 0xA1, 0x09, 0x07, 0xB8, 0xA4} , // 2
    {0xE4, 0x86, 0xEE, 0xE3, 0xD0, 0xC0, 0x9C, 0x90, 0x2F, 0x66, 0x86, 0xD4, 0xC0, 0x6F, 0x64, 0x9F} , // 3
    {0xED, 0x31, 0xBA, 0x9C, 0x04, 0xB0, 0x67, 0x50, 0x6C, 0x44, 0x97, 0xA3, 0x5B, 0x78, 0x04, 0xFC} , // 4
    {0x5E, 0x66, 0x99, 0x8A, 0xB4, 0xE8, 0x93, 0x16, 0x06, 0x85, 0x0F, 0xD7, 0xA1, 0x6D, 0xD7, 0x55} , // 5
};

// see: http://3dbrew.org/wiki/Flash_Filesystem
static PartitionInfo partitions[] = {
    { "TWLN",    {0xE9, 0x00, 0x00, 0x54, 0x57, 0x4C, 0x20, 0x20}, 0x00012E00, 0x08FB5200, 0x3, AES_CNT_TWLNAND_MODE },
    { "TWLP",    {0xE9, 0x00, 0x00, 0x54, 0x57, 0x4C, 0x20, 0x20}, 0x09011A00, 0x020B6600, 0x3, AES_CNT_TWLNAND_MODE },
    { "AGBSAVE", {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 0x0B100000, 0x00030000, 0x7, AES_CNT_CTRNAND_MODE },
    { "FIRM0",   {0x46, 0x49, 0x52, 0x4D, 0x00, 0x00, 0x00, 0x00}, 0x0B130000, 0x00400000, 0x6, AES_CNT_CTRNAND_MODE },
    { "FIRM1",   {0x46, 0x49, 0x52, 0x4D, 0x00, 0x00, 0x00, 0x00}, 0x0B530000, 0x00400000, 0x6, AES_CNT_CTRNAND_MODE },
    { "CTRNAND", {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20}, 0x0B95CA00, 0x2F3E3600, 0x4, AES_CNT_CTRNAND_MODE }, // O3DS
    { "CTRNAND", {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20}, 0x0B95AE00, 0x41D2D200, 0x5, AES_CNT_CTRNAND_MODE }  // N3DS
};

u32 DecryptBuffer(DecryptBufferInfo *info)
{
    u8 ctr[16] __attribute__((aligned(32)));
    memcpy(ctr, info->CTR, 16);

    u8* buffer = info->buffer;
    u32 size = info->size;
    u32 mode = info->mode;

    if (info->setKeyY) {
        u8 keyY[16] __attribute__((aligned(32)));
        memcpy(keyY, info->keyY, 16);
        setup_aeskey(info->keyslot, AES_BIG_INPUT | AES_NORMAL_INPUT, keyY);
        info->setKeyY = 0;
    }
    use_aeskey(info->keyslot);

    for (u32 i = 0; i < size; i += 0x10, buffer += 0x10) {
        set_ctr(ctr);
        aes_decrypt((void*) buffer, (void*) buffer, ctr, 1, mode);
        add_ctr(ctr, 0x1);
    }

    memcpy(info->CTR, ctr, 16);
    
    return 0;
}

u32 DecryptTitlekey(TitleKeyEntry* entry)
{
    DecryptBufferInfo info = {.keyslot = 0x3D, .setKeyY = 1, .size = 16, .buffer = entry->encryptedTitleKey, .mode = AES_CNT_TITLEKEY_MODE};
    memset(info.CTR, 0, 16);
    memcpy(info.CTR, entry->titleId, 8);
    memcpy(info.keyY, (void *)common_keyy[entry->commonKeyIndex], 16);
    
    DecryptBuffer(&info);
    
    return 0;
}

u32 DumpTicket() {
    PartitionInfo* ctrnand_info = &(partitions[(GetUnitPlatform() == PLATFORM_3DS) ? 5 : 6]);
    u32 offset;
    u32 size;
    
    Debug("Searching for ticket.db...");
    if (SeekFileInNand(&offset, &size, NULL, "TICKET  DB ", ctrnand_info) != 0) {
        Debug("Failed!");
        return 1;
    }
    Debug("Found at %08X, size %uMB", offset, size / (1024 * 1024));
    
    if (DecryptNandToFile("ticket.db", offset, size, ctrnand_info) != 0)
        return 1;
    
    return 0;
}

u32 DecryptTitlekeysFile(void)
{
    EncKeysInfo *info = (EncKeysInfo*)0x20316000;

    if (!DebugFileOpen("/encTitleKeys.bin"))
        return 1;
    
    if (!DebugFileRead(info, 16, 0)) {
        FileClose();
        return 1;
    }

    if (!info->n_entries || info->n_entries > MAX_ENTRIES) {
        Debug("Too many/few entries specified: %i", info->n_entries);
        FileClose();
        return 1;
    }

    Debug("Number of entries: %i", info->n_entries);
    if (!DebugFileRead(info->entries, info->n_entries * sizeof(TitleKeyEntry), 16)) {
        FileClose();
        return 1;
    }
    
    FileClose();

    Debug("Decrypting Title Keys...");
    for (u32 i = 0; i < info->n_entries; i++)
        DecryptTitlekey(&(info->entries[i]));

    if (!DebugFileCreate("/decTitleKeys.bin", true))
        return 1;
    if (!DebugFileWrite(info, info->n_entries * sizeof(TitleKeyEntry) + 16, 0)) {
        FileClose();
        return 1;
    }
    FileClose();

    return 0;
}

u32 DecryptTitlekeysNand(void)
{
    PartitionInfo* ctrnand_info = &(partitions[(GetUnitPlatform() == PLATFORM_3DS) ? 5 : 6]);
    u8* buffer = BUFFER_ADDRESS;
    EncKeysInfo *info = (EncKeysInfo*) 0x20316000;
    
    u32 nKeys = 0;
    u32 offset = 0;
    u32 size = 0;
    
    Debug("Searching for ticket.db...");
    if (SeekFileInNand(&offset, &size, NULL, "TICKET  DB ", ctrnand_info) != 0) {
        Debug("Failed!");
        return 1;
    }
    Debug("Found at %08X, size %uMB", offset, size / (1024 * 1024));
    
    Debug("Decrypting Title Keys...");
    memset(info, 0, 0x10);
    for (u32 t_offset = 0; t_offset < size; t_offset += NAND_SECTOR_SIZE * (SECTORS_PER_READ-1)) {
        u32 read_bytes = min(NAND_SECTOR_SIZE * SECTORS_PER_READ, (size - t_offset));
        ShowProgress(t_offset, size);
        DecryptNandToMem(buffer, offset + t_offset, read_bytes, ctrnand_info);
        for (u32 i = 0; i < read_bytes - NAND_SECTOR_SIZE; i++) {
            if(memcmp(buffer + i, (u8*) "Root-CA00000003-XS0000000c", 26) == 0) {
                u32 exid;
                u8* titleId = buffer + i + 0x9C;
                u32 commonKeyIndex = *(buffer + i + 0xB1);
                u8* titlekey = buffer + i + 0x7F;
                for (exid = 0; exid < nKeys; exid++)
                    if (memcmp(titleId, info->entries[exid].titleId, 8) == 0)
                        break;
                if (exid < nKeys)
                    continue; // continue if already dumped
                memset(&(info->entries[nKeys]), 0, sizeof(TitleKeyEntry));
                memcpy(info->entries[nKeys].titleId, titleId, 8);
                memcpy(info->entries[nKeys].encryptedTitleKey, titlekey, 16);
                info->entries[nKeys].commonKeyIndex = commonKeyIndex;
                DecryptTitlekey(&(info->entries[nKeys]));
                nKeys++;
            }
        }
        if (nKeys == MAX_ENTRIES) {
            Debug("Maximum number of titlekeys found");
            break;
        }
    }
    info->n_entries = nKeys;
    ShowProgress(0, 0);
    
    Debug("Decrypted %u unique Title Keys", nKeys);
    
    if(nKeys > 0) {
        if (!DebugFileCreate("/decTitleKeys.bin", true))
            return 1;
        if (!DebugFileWrite(info, 0x10 + nKeys * 0x20, 0)) {
            FileClose();
            return 1;
        }
        FileClose();
    } else {
        return 1;
    }

    return 0;
}

u32 NcchPadgen()
{
    u32 result;

    NcchInfo *info = (NcchInfo*)0x20316000;
    SeedInfo *seedinfo = (SeedInfo*)0x20400000;

    if (DebugFileOpen("/slot0x25KeyX.bin")) {
        u8 slot0x25KeyX[16] = {0};
        if (!DebugFileRead(&slot0x25KeyX, 16, 0)) {
            FileClose();
            return 1;
        }
        FileClose();
        setup_aeskeyX(0x25, slot0x25KeyX);
    } else {
        Debug("7.x game decryption will fail on less than 7.x!");
    }

    if (DebugFileOpen("/seeddb.bin")) {
        if (!DebugFileRead(seedinfo, 16, 0)) {
            FileClose();
            return 1;
        }
        if (!seedinfo->n_entries || seedinfo->n_entries > MAX_ENTRIES) {
            FileClose();
            Debug("Too many/few seeddb entries.");
            return 1;
        }
        if (!DebugFileRead(seedinfo->entries, seedinfo->n_entries * sizeof(SeedInfoEntry), 16)) {
            FileClose();
            return 1;
        }
        FileClose();
    } else {
        Debug("9.x seed crypto game decryption will fail!");
    }

    if (!DebugFileOpen("/ncchinfo.bin"))
        return 1;
    if (!DebugFileRead(info, 16, 0)) {
        FileClose();
        return 1;
    }

    if (!info->n_entries || info->n_entries > MAX_ENTRIES) {
        FileClose();
        Debug("Too many/few entries in ncchinfo.bin");
        return 1;
    }
    if (info->ncch_info_version != 0xF0000004) {
        FileClose();
        Debug("Wrong version ncchinfo.bin");
        return 1;
    }
    if (!DebugFileRead(info->entries, info->n_entries * sizeof(NcchInfoEntry), 16)) {
        FileClose();
        return 1;
    }
    FileClose();

    Debug("Number of entries: %i", info->n_entries);

    for(u32 i = 0; i < info->n_entries; i++) {
        Debug("Creating pad number: %i. Size (MB): %i", i+1, info->entries[i].size_mb);

        PadInfo padInfo = {.setKeyY = 1, .size_mb = info->entries[i].size_mb, .mode = AES_CNT_CTRNAND_MODE};
        memcpy(padInfo.CTR, info->entries[i].CTR, 16);
        memcpy(padInfo.filename, info->entries[i].filename, 112);
        if (info->entries[i].usesSeedCrypto) {
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
                return 1;
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
        else if(info->entries[i].uses7xCrypto >> 8 == 0xDEC0DE) // magic value to manually specify keyslot
            padInfo.keyslot = info->entries[i].uses7xCrypto & 0x3F;
        else if(info->entries[i].uses7xCrypto)
            padInfo.keyslot = 0x25;
        else
            padInfo.keyslot = 0x2C;
        Debug("Using keyslot: %02X", padInfo.keyslot);

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
    u32 result;

    SdInfo *info = (SdInfo*)0x20316000;

    u8 movable_seed[0x120] = {0};

    // Load console 0x34 keyY from movable.sed if present on SD card
    if (DebugFileOpen("/movable.sed")) {
        if (!DebugFileRead(&movable_seed, 0x120, 0)) {
            FileClose();
            return 1;
        }
        FileClose();
        if (memcmp(movable_seed, "SEED", 4) != 0) {
            Debug("movable.sed is too corrupt!");
            return 1;
        }
        setup_aeskey(0x34, AES_BIG_INPUT|AES_NORMAL_INPUT, &movable_seed[0x110]);
        use_aeskey(0x34);
    }

    if (!DebugFileOpen("/SDinfo.bin"))
        return 1;
    if (!DebugFileRead(info, 4, 0)) {
        FileClose();
        return 1;
    }

    if (!info->n_entries || info->n_entries > MAX_ENTRIES) {
        FileClose();
        Debug("Too many/few entries!");
        return 1;
    }

    Debug("Number of entries: %i", info->n_entries);

    if (!DebugFileRead(info->entries, info->n_entries * sizeof(SdInfoEntry), 4)) {
        FileClose();
        return 1;
    }
    FileClose();

    for(u32 i = 0; i < info->n_entries; i++) {
        Debug ("Creating pad number: %i. Size (MB): %i", i+1, info->entries[i].size_mb);

        PadInfo padInfo = {.keyslot = 0x34, .setKeyY = 0, .size_mb = info->entries[i].size_mb, .mode = AES_CNT_CTRNAND_MODE};
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

u32 GetNandCtr(u8* ctr, u32 offset)
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
    static u8* ctr_start = NULL;
    
    if (ctr_start == NULL) {
        for (u32 i = 0; i < version_ctrs_len; i++) {
            if (*(u32*)version_ctrs[i] == 0x5C980) {
                Debug("System version %s", versions[i]);
                ctr_start = (u8*) version_ctrs[i] + 0x30;
            }
        }
        
        // If value not in previous list start memory scanning (test range)
        if (ctr_start == NULL) {
            for (u8* c = (u8*) 0x080D8FFF; c > (u8*) 0x08000000; c--) {
                if (*(u32*)c == 0x5C980 && *(u32*)(c + 1) == 0x800005C9) {
                    ctr_start = c + 0x30;
                    Debug("CTR Start 0x%08X", ctr_start);
                    break;
                }
            }
        }
        
        if (ctr_start == NULL) {
            Debug("CTR Start not found!");
            return 1;
        }
    }
    
    // the CTR is stored backwards in memory
    if (offset >= 0x0B100000) { // CTRNAND/AGBSAVE region
        for (u32 i = 0; i < 16; i++)
            ctr[i] = *(ctr_start + (0xF - i));
    } else { // TWL region
        for (u32 i = 0; i < 16; i++)
            ctr[i] = *(ctr_start + 0x88 + (0xF - i));
    }
    
    // increment counter
    add_ctr(ctr, offset / 0x10);

    return 0;
}

u32 SeekFileInNand(u32* offset, u32* size, u32* seekpos, const char* filename, PartitionInfo* partition)
{
    // poor mans NAND FAT file seeker:
    // - can't handle long filenames
    // - filename must be in FAT 8+3 format
    // - doesn't search the root dir
    // - dirs must not exceed 1024 entries
    // - fragmentation not supported
    
    const static char* magic = ".          ";
    const static char zeroes[8+3] = { 0x00 };
    u8* buffer = BUFFER_ADDRESS;
    u32 p_size = partition->size;
    u32 p_offset = partition->offset;
    
    u32 cluster_size;
    u32 cluster_start;
    bool found = false;
    
    if (strnlen(filename, 16) != 8+3)
        return 1;
    
    DecryptNandToMem(buffer, p_offset, NAND_SECTOR_SIZE, partition);
    
    // good FAT header description found here: http://www.compuphase.com/mbr_fat.htm
    u32 fat_start = NAND_SECTOR_SIZE * (*((u16*) (buffer + 0x0E)));
    u32 fat_size = NAND_SECTOR_SIZE * (*((u16*) (buffer + 0x16)) * buffer[0x10]);
    u32 root_size = *((u16*) (buffer + 0x11)) * 0x20;
    cluster_start = fat_start + fat_size + root_size;
    cluster_size = buffer[0x0D] * NAND_SECTOR_SIZE;
    
    if (seekpos != NULL && cluster_start > *seekpos)
        *seekpos = cluster_start;
    
    for (u32 i = (seekpos == NULL) ? cluster_start : *seekpos; i < p_size; i += cluster_size) {
        DecryptNandToMem(buffer, p_offset + i, NAND_SECTOR_SIZE, partition);
        if (memcmp(buffer, magic, 8+3) != 0)
            continue;
        DecryptNandToMem(buffer, p_offset + i, cluster_size, partition);
        for (u32 j = 0; j < cluster_size; j += 0x20) {
            if (memcmp(buffer + j, filename, 8+3) == 0) {
                *offset = p_offset + cluster_start + (*((u16*) (buffer + j + 0x1A)) - 2) * cluster_size;
                *size = *((u32*) (buffer + j + 0x1C));
                if (*size > 0) {
                    found = true;
                    if (seekpos != NULL)
                        *seekpos = i + cluster_size;
                    break;
                }
            } else if (memcmp(buffer + j, zeroes, 8+3) == 0)
                break;
        }
        if (found) break;
    }
    
    return (found) ? 0 : 1;
}

u32 DecryptNandToMem(u8* buffer, u32 offset, u32 size, PartitionInfo* partition)
{
    DecryptBufferInfo info = {.keyslot = partition->keyslot, .setKeyY = 0, .size = size, .buffer = buffer, .mode = partition->mode};
    if(GetNandCtr(info.CTR, offset) != 0)
        return 1;

    u32 n_sectors = (size + NAND_SECTOR_SIZE - 1) / NAND_SECTOR_SIZE;
    u32 start_sector = offset / NAND_SECTOR_SIZE;
    sdmmc_nand_readsectors(start_sector, n_sectors, buffer);
    DecryptBuffer(&info);

    return 0;
}

u32 DecryptNandToFile(const char* filename, u32 offset, u32 size, PartitionInfo* partition)
{
    u8* buffer = BUFFER_ADDRESS;
    u32 result = 0;

    if (!DebugFileCreate(filename, true))
        return 1;

    for (u32 i = 0; i < size; i += NAND_SECTOR_SIZE * SECTORS_PER_READ) {
        u32 read_bytes = min(NAND_SECTOR_SIZE * SECTORS_PER_READ, (size - i));
        ShowProgress(i, size);
        DecryptNandToMem(buffer, offset + i, read_bytes, partition);
        if(!DebugFileWrite(buffer, read_bytes, i)) {
            result = 1;
            break;
        }
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}

u32 CtrNandPadgen()
{
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

    PadInfo padInfo = {.keyslot = keyslot, .setKeyY = 0, .size_mb = nand_size, .filename = "nand.fat16.xorpad", .mode = AES_CNT_CTRNAND_MODE};
    if(GetNandCtr(padInfo.CTR, 0xB930000) != 0)
        return 1;

    return CreatePad(&padInfo);
}

u32 TwlNandPadgen()
{
    u32 size_mb = (partitions[0].size + (1024 * 1024) - 1) / (1024 * 1024);
    Debug("Creating TWLNAND FAT16 xorpad. Size (MB): %u", size_mb);
    Debug("Filename: twlnand.fat16.xorpad");

    PadInfo padInfo = {
        .keyslot = partitions[0].keyslot,
        .setKeyY = 0,
        .size_mb = size_mb,
        .filename = "twlnand.fat16.xorpad",
        .mode = AES_CNT_TWLNAND_MODE};
    if(GetNandCtr(padInfo.CTR, partitions[0].offset) != 0)
        return 1;

    return CreatePad(&padInfo);
}

u32 CreatePad(PadInfo *info)
{
    u8* buffer = BUFFER_ADDRESS;
    u32 result = 0;
    
    if (!FileCreate(info->filename, true)) // No DebugFileCreate() here - messages are already given
        return 1;
        
    DecryptBufferInfo decryptInfo = {.keyslot = info->keyslot, .setKeyY = info->setKeyY, .mode = info->mode, .buffer = buffer};
    memcpy(decryptInfo.CTR, info->CTR, 16);
    memcpy(decryptInfo.keyY, info->keyY, 16);
    u32 size_bytes = info->size_mb * 1024*1024;
    for (u32 i = 0; i < size_bytes; i += BUFFER_MAX_SIZE) {
        u32 curr_block_size = min(BUFFER_MAX_SIZE, size_bytes - i);
        decryptInfo.size = curr_block_size;
        memset(buffer, 0x00, curr_block_size);
        ShowProgress(i, size_bytes);
        DecryptBuffer(&decryptInfo);
        if (!DebugFileWrite((void*)buffer, curr_block_size, i)) {
            result = 1;
            break;
        }
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}

u32 DumpNand()
{
    u8* buffer = BUFFER_ADDRESS;
    u32 nand_size = (GetUnitPlatform() == PLATFORM_3DS) ? 0x3AF00000 : 0x4D800000;
    u32 result = 0;

    Debug("Dumping System NAND. Size (MB): %u", nand_size / (1024 * 1024));

    if (!DebugFileCreate("/NAND.bin", true))
        return 1;

    u32 n_sectors = nand_size / NAND_SECTOR_SIZE;
    for (u32 i = 0; i < n_sectors; i += SECTORS_PER_READ) {
        ShowProgress(i, n_sectors);
        sdmmc_nand_readsectors(i, SECTORS_PER_READ, buffer);
        if(!DebugFileWrite(buffer, NAND_SECTOR_SIZE * SECTORS_PER_READ, i * NAND_SECTOR_SIZE)) {
            result = 1;
            break;
        }
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}

u32 DecryptNandPartition(PartitionInfo* p) {
    char filename[32];
    u8 magic[NAND_SECTOR_SIZE];
    
    Debug("Dumping & Decrypting %s, size (MB): %u", p->name, p->size / (1024 * 1024));
    if (DecryptNandToMem(magic, p->offset, 16, p) != 0)
        return 1;
    if ((p->magic[0] != 0xFF) && (memcmp(p->magic, magic, 8) != 0)) {
        Debug("Decryption error, please contact us");
        return 1;
    }
    snprintf(filename, 32, "%s.bin", p->name);
    
    return DecryptNandToFile(filename, p->offset, p->size, p);
}

u32 DecryptTwlAgbPartitions() {
    u32 result = 0;
    
    result |= DecryptNandPartition(&(partitions[0])); // TWLN
    result |= DecryptNandPartition(&(partitions[1])); // TWLP
    result |= DecryptNandPartition(&(partitions[2])); // AGBSAVE
    
    return result;
}
    
u32 DecryptCtrPartitions() {
    u32 result = 0;
    bool o3ds = (GetUnitPlatform() == PLATFORM_3DS);
    
    result |= DecryptNandPartition(&(partitions[3])); // FIRM0
    result |= DecryptNandPartition(&(partitions[4])); // FIRM1
    result |= DecryptNandPartition(&(partitions[(o3ds) ? 5 : 6])); // CTRNAND O3DS / N3DS

    return result;
}
