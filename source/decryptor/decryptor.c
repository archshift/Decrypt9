#include <string.h>
#include <stdio.h>

#include "fs.h"
#include "draw.h"
#include "platform.h"
#include "decryptor/decryptor.h"
#include "decryptor/crypto.h"
#include "decryptor/features.h"
#include "decryptor/sha256.h"
#include "fatfs/sdmmc.h"

#define BUFFER_ADDRESS  ((u8*) 0x21000000)
#define BUFFER_MAX_SIZE (1 * 1024 * 1024)

#define NAND_SECTOR_SIZE 0x200
#define SECTORS_PER_READ (BUFFER_MAX_SIZE / NAND_SECTOR_SIZE)

#define DECRYPT_DIR "D9decrypt"


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

static u32 emunand_header = 0;
static u32 emunand_offset = 0;


u32 SetNand(u32 use_emunand)
{
    u8* buffer = BUFFER_ADDRESS;
    
    if (use_emunand) {
        u32 nand_size_sectors = getMMCDevice(0)->total_size;
        // check for Gateway type EmuNAND
        sdmmc_sdcard_readsectors(nand_size_sectors, 1, buffer);
        if (memcmp(buffer + 0x100, "NCSD", 4) == 0) {
            emunand_header = nand_size_sectors;
            emunand_offset = 0;
            Debug("Using EmuNAND @ %06X/%06X", emunand_header, emunand_offset);
            return 0;
        }
        // check for RedNAND type EmuNAND
        sdmmc_sdcard_readsectors(1, 1, buffer);
        if (memcmp(buffer + 0x100, "NCSD", 4) == 0) {
            emunand_header = 1;
            emunand_offset = 1;
            Debug("Using RedNAND @ %06X/%06X", emunand_header, emunand_offset);
            return 0;
        }
        // no EmuNAND found
        Debug("EmuNAND is not available");
        return 1;
    } else {
        emunand_header = 0;
        emunand_offset = 0;
        return 0;
    }
}

static inline int ReadNandSectors(u32 sector_no, u32 numsectors, u8 *out)
{
    if (emunand_header) {
        if (sector_no == 0) {
            int errorcode = sdmmc_sdcard_readsectors(emunand_header, 1, out);
            if (errorcode) return errorcode;
            sector_no = 1;
            numsectors--;
            out += 0x200;
        }
        return sdmmc_sdcard_readsectors(sector_no + emunand_offset, numsectors, out);
    } else return sdmmc_nand_readsectors(sector_no, numsectors, out);
}

static inline int WriteNandSectors(u32 sector_no, u32 numsectors, u8 *in)
{
    if (emunand_header) {
        if (sector_no == 0) {
            int errorcode = sdmmc_sdcard_writesectors(emunand_header, 1, in);
            if (errorcode) return errorcode;
            sector_no = 1;
            numsectors--;
            in += 0x200;
        }
        return sdmmc_sdcard_writesectors(sector_no + emunand_offset, numsectors, in);
    } else return sdmmc_nand_writesectors(sector_no, numsectors, in);
}

u32 CryptBuffer(CryptBufferInfo *info)
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
    CryptBufferInfo info = {.keyslot = 0x3D, .setKeyY = 1, .size = 16, .buffer = entry->encryptedTitleKey, .mode = AES_CNT_TITLEKEY_MODE};
    memset(info.CTR, 0, 16);
    memcpy(info.CTR, entry->titleId, 8);
    memcpy(info.keyY, (void *)common_keyy[entry->commonKeyIndex], 16);
    
    CryptBuffer(&info);
    
    return 0;
}

u32 DumpSeedsave()
{
    PartitionInfo* ctrnand_info = &(partitions[(GetUnitPlatform() == PLATFORM_3DS) ? 5 : 6]);
    u32 offset;
    u32 size;
    
    Debug("Searching for seedsave...");
    if (SeekFileInNand(&offset, &size, "DATA       ???????????SYSDATA    0001000F   00000000   ", ctrnand_info) != 0) {
        Debug("Failed!");
        return 1;
    }
    Debug("Found at %08X, size %ukB", offset, size / 1024);
    
    if (DecryptNandToFile("/seedsave.bin", offset, size, ctrnand_info) != 0)
        return 1;
    
    return 0;
}

u32 UpdateSeedDb()
{
    PartitionInfo* ctrnand_info = &(partitions[(GetUnitPlatform() == PLATFORM_3DS) ? 5 : 6]);
    u8* buffer = BUFFER_ADDRESS;
    SeedInfo *seedinfo = (SeedInfo*) 0x20400000;
    
    u32 nNewSeeds = 0;
    u32 offset;
    u32 size;
    
    // load full seedsave to memory
    Debug("Searching for seedsave...");
    if (SeekFileInNand(&offset, &size, "DATA       ???????????SYSDATA    0001000F   00000000   ", ctrnand_info) != 0) {
        Debug("Failed!");
        return 1;
    }
    Debug("Found at %08X, size %ukB", offset, size / 1024);
    if (size != 0xAC000) {
        Debug("Expected %ukB, failed!", 0xAC000);
        return 1;
    }
    DecryptNandToMem(buffer, offset, size, ctrnand_info);
    
    // load / create seeddb.bin
    if (DebugFileOpen("/seeddb.bin")) {
        if (!DebugFileRead(seedinfo, 16, 0)) {
            FileClose();
            return 1;
        }
        if (seedinfo->n_entries > MAX_ENTRIES) {
            Debug("seeddb.bin seems to be corrupt!");
            FileClose();
            return 1;
        }
        if (!DebugFileRead(seedinfo->entries, seedinfo->n_entries * sizeof(SeedInfoEntry), 16)) {
            FileClose();
            return 1;
        }
    } else {
        if (!DebugFileCreate("/seeddb.bin", true))
            return 1;
        memset(seedinfo, 0x00, 16);
    }
    
    // search and extract seeds
    for ( size_t i = 0; i < 2000; i++ ) {
        static const u8 magic[4] = { 0x00, 0x00, 0x04, 0x00 };
        u8* titleId = buffer + 0x7000 + (i*8);
        u8* seed = buffer + 0x7000 + (2000*8) + (i*16);
        if (memcmp(titleId + 4, magic, 4) != 0) continue;
        // seed found, check if it already exists
        u32 entryPos = 0;
        for (entryPos = 0; entryPos < seedinfo->n_entries; entryPos++)
            if (memcmp(titleId, &(seedinfo->entries[entryPos].titleId), 8) == 0) break;
        if (entryPos < seedinfo->n_entries) {
            Debug("Found %08X%08X seed (duplicate)", *((u32*) (titleId + 4)), *((u32*) titleId));
            continue;
        }
        // seed is new, create a new entry
        Debug("Found %08X%08X seed (new)", *((u32*) (titleId + 4)), *((u32*) titleId));
        memset(&(seedinfo->entries[entryPos]), 0x00, sizeof(SeedInfoEntry));
        memcpy(&(seedinfo->entries[entryPos].titleId), titleId, 8);
        memcpy(&(seedinfo->entries[entryPos].external_seed), seed, 16);
        seedinfo->n_entries++;
        nNewSeeds++;
    }
    
    if (nNewSeeds == 0) {
        Debug("Found no new seeds, %i total", seedinfo->n_entries);
        FileClose();
        return 1;
    }
    
    Debug("Found %i new seeds, %i total", nNewSeeds, seedinfo->n_entries);
    if (!DebugFileWrite(seedinfo, 16 + seedinfo->n_entries * sizeof(SeedInfoEntry), 0))
        return 1;
    FileClose();
    
    return 0;
}

u32 DumpTicket() {
    PartitionInfo* ctrnand_info = &(partitions[(GetUnitPlatform() == PLATFORM_3DS) ? 5 : 6]);
    u32 offset;
    u32 size;
    
    Debug("Searching for ticket.db...");
    if (SeekFileInNand(&offset, &size, "DBS        TICKET  DB ", ctrnand_info) != 0) {
        Debug("Failed!");
        return 1;
    }
    Debug("Found at %08X, size %uMB", offset, size / (1024 * 1024));
    
    if (DecryptNandToFile((emunand_header) ? "/ticket_emu.db" : "/ticket.db", offset, size, ctrnand_info) != 0)
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
    if (SeekFileInNand(&offset, &size, "DBS        TICKET  DB ", ctrnand_info) != 0) {
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
        if (!DebugFileCreate((emunand_header) ? "/decTitleKeys_emu.bin" : "/decTitleKeys.bin", true))
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
    if (info->ncch_info_version == 0xF0000004) { // ncchinfo v4
        if (!DebugFileRead(info->entries, info->n_entries * sizeof(NcchInfoEntry), 16)) {
            FileClose();
            return 1;
        }
    } else if (info->ncch_info_version == 0xF0000003) { // ncchinfo v3
        // read ncchinfo v3 entry & convert to ncchinfo v4
        for (u32 i = 0; i < info->n_entries; i++) {
            u8* entry_data = (u8*) (info->entries + i);
            if (!DebugFileRead(entry_data, 160, 16 + (160*i))) {
                FileClose();
                return 1;
            }
            memmove(entry_data + 56, entry_data + 48, 112);
            *(u64*) (entry_data + 48) = 0;
        }
    } else { // unknown file / ncchinfo version
        FileClose();
        Debug("Incompatible version ncchinfo.bin");
        return 1;
    }
    FileClose();

    Debug("Number of entries: %i", info->n_entries);

    for (u32 i = 0; i < info->n_entries; i++) { // check and fix filenames
        char* filename = info->entries[i].filename;
        if (filename[1] == 0x00) { // convert UTF-16 -> UTF-8
            for (u32 j = 1; j < (112 / 2); j++)
                filename[j] = filename[j*2];
        }
        if (memcmp(filename, "sdmc:", 5) == 0) // fix sdmc: prefix
            memmove(filename, filename + 5, 112 - 5);
    }
            
    for (u32 i = 0; i < info->n_entries; i++) {
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

        if (info->entries[i].uses7xCrypto == 0xA) { 
            if (GetUnitPlatform() == PLATFORM_3DS) { // won't work on an Old 3DS
                Debug("This can only be generated on N3DS!");
                return 1;
            }
            padInfo.keyslot = 0x18;
        } else if (info->entries[i].uses7xCrypto == 0xB) {
            Debug("Secure4 xorpad cannot be generated yet!");
            return 1;
        } else if(info->entries[i].uses7xCrypto >> 8 == 0xDEC0DE) // magic value to manually specify keyslot
            padInfo.keyslot = info->entries[i].uses7xCrypto & 0x3F;
        else if (info->entries[i].uses7xCrypto)
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

u32 SeekFileInNand(u32* offset, u32* size, const char* path, PartitionInfo* partition)
{
    // poor mans NAND FAT file seeker:
    // - path must be in FAT 8+3 format, without dots or slashes
    //   example: DIR1_______DIR2_______FILENAMEEXT
    // - can't handle long filenames
    // - dirs must not exceed 1024 entries
    // - fragmentation not supported
    
    const static char zeroes[8+3] = { 0x00 };
    u8* buffer = BUFFER_ADDRESS;
    u32 p_size = partition->size;
    u32 p_offset = partition->offset;
    
    u32 cluster_size;
    u32 cluster_start;
    bool found = false;
    
    if (strnlen(path, 256) % (8+3) != 0)
        return 1;
    
    DecryptNandToMem(buffer, p_offset, NAND_SECTOR_SIZE, partition);
    
    // good FAT header description found here: http://www.compuphase.com/mbr_fat.htm
    u32 fat_start = NAND_SECTOR_SIZE * (*((u16*) (buffer + 0x0E)));
    u32 fat_size = NAND_SECTOR_SIZE * (*((u16*) (buffer + 0x16)) * buffer[0x10]);
    u32 root_size = *((u16*) (buffer + 0x11)) * 0x20;
    cluster_start = fat_start + fat_size + root_size;
    cluster_size = buffer[0x0D] * NAND_SECTOR_SIZE;
    
    for (*offset = p_offset + fat_start + fat_size; strnlen(path, 256) >= 8+3; path += 8+3) {
        if (*offset - p_offset > p_size)
            return 1;
        found = false;
        DecryptNandToMem(buffer, *offset, cluster_size, partition);
        for (u32 i = 0x00; i < cluster_size; i += 0x20) {
            // skip invisible, deleted and lfn entries
            if ((buffer[i] == '.') || (buffer[i] == 0xE5) || (buffer[i+0x0B] == 0x0F))
                continue;
            else if (memcmp(buffer + i, zeroes, 8+3) == 0)
                return 1;
            u32 p; // search for path in fat folder structure, accept '?' wildcards
            for (p = 0; (p < 8+3) && (path[p] == '?' || buffer[i+p] == path[p]); p++);
            if (p != 8+3) continue;
            // entry found, store offset and move on
            *offset = p_offset + cluster_start + (*((u16*) (buffer + i + 0x1A)) - 2) * cluster_size;
            *size = *((u32*) (buffer + i + 0x1C));
            found = true;
            break;
        }
        if (!found) break;
    }
    
    return (found) ? 0 : 1;
}

u32 DecryptNandToMem(u8* buffer, u32 offset, u32 size, PartitionInfo* partition)
{
    CryptBufferInfo info = {.keyslot = partition->keyslot, .setKeyY = 0, .size = size, .buffer = buffer, .mode = partition->mode};
    if(GetNandCtr(info.CTR, offset) != 0)
        return 1;

    u32 n_sectors = (size + NAND_SECTOR_SIZE - 1) / NAND_SECTOR_SIZE;
    u32 start_sector = offset / NAND_SECTOR_SIZE;
    ReadNandSectors(start_sector, n_sectors, buffer);
    CryptBuffer(&info);

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

u32 DecryptSdToSd(const char* filename, u32 offset, u32 size, CryptBufferInfo* info)
{
    u8* buffer = BUFFER_ADDRESS;
    u32 offset_16 = offset % 16;
    u32 result = 0;

    // No DebugFileOpen() - at this point the file has already been checked enough
    if (!FileOpen(filename)) 
        return 1;

    info->buffer = buffer;
    if (offset_16) { // handle offset alignment / this assumes the data is >= 16 byte
        if(!DebugFileRead(buffer + offset_16, 16 - offset_16, offset)) {
            result = 1;
        }
        info->size = 16;
        CryptBuffer(info);
        if(!DebugFileWrite(buffer + offset_16, 16 - offset_16, offset)) {
            result = 1;
        }
    }
    for (u32 i = (offset_16) ? (16 - offset_16) : 0; i < size; i += BUFFER_MAX_SIZE) {
        u32 read_bytes = min(BUFFER_MAX_SIZE, (size - i));
        ShowProgress(i, size);
        if(!DebugFileRead(buffer, read_bytes, offset + i)) {
            result = 1;
            break;
        }
        info->size = read_bytes;
        CryptBuffer(info);
        if(!DebugFileWrite(buffer, read_bytes, offset + i)) {
            result = 1;
            break;
        }
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}

u32 CheckHash(const char* filename, u32 offset, u32 size, u8* hash)
{
    // uses the standard buffer, so be careful
    u8* buffer = BUFFER_ADDRESS; 
    u8 digest[32];
    sha256_context shactx;
    
    if (!FileOpen(filename))
        return 1;
    sha256_starts(&shactx);
    for (u32 i = 0; i < size; i += BUFFER_MAX_SIZE) {
        u32 read_bytes = min(BUFFER_MAX_SIZE, (size - i));
        if(!FileRead(buffer, read_bytes, offset + i)) {
            FileClose();
            return 1;
        }
        sha256_update(&shactx, buffer, read_bytes);
    }
    sha256_finish(&shactx, digest);
    FileClose();
    
    return (memcmp(hash, digest, 32) == 0) ? 0 : 1; 
}

u32 DecryptNcch(const char* filename, u32 offset)
{
    NcchHeader* ncch = (NcchHeader*) 0x20316200;
    u8* buffer = (u8*) 0x20316400;
    CryptBufferInfo info0 = {.setKeyY = 1, .keyslot = 0x2C, .mode = AES_CNT_CTRNAND_MODE};
    CryptBufferInfo info1 = {.setKeyY = 1, .mode = AES_CNT_CTRNAND_MODE};
    u8 seedKeyY[16] = { 0x00 };
    u32 result = 0;
    
    if (!FileOpen(filename)) // already checked this file
        return 1;
    if (!DebugFileRead((void*) ncch, 0x200, offset)) {
        FileClose();
        return 1;
    }
    FileClose();
    
    // check if encrypted
    if (ncch->flags[7] & 0x04) {
        Debug("NCCH is not encrypted");
        return 0;
    }
    
    bool uses7xCrypto = ncch->flags[3];
    bool usesSeedCrypto = ncch->flags[7] & 0x20;
    bool usesSec3Crypto = (ncch->flags[3] == 0x0A);
    bool usesSec4Crypto = (ncch->flags[3] == 0x0B);
    
    Debug("Code / Crypto: %s / %s%s%s", ncch->productCode, (usesSec4Crypto) ? "Secure4 " : (usesSec3Crypto) ? "Secure3 " : (uses7xCrypto) ? "7x " : "", (usesSeedCrypto) ? "Seed " : "", (!uses7xCrypto && !usesSeedCrypto) ? "Standard" : "");
   
    // check secure4 crypto
    if (usesSec4Crypto) {
        Debug("Secure4 cannot be decrypted yet!");
        return 1;
    }
    
    // check / setup 7x crypto
    if (uses7xCrypto && (GetUnitPlatform() == PLATFORM_3DS)) {
        if (usesSec3Crypto) {
            Debug("Can only be decrypted on N3DS!");
            return 1;
        }
        if (FileOpen("slot0x25KeyX.bin")) {
            u8 slot0x25KeyX[16] = {0};
            if (FileRead(&slot0x25KeyX, 16, 0) != 16) {
                Debug("slot0x25keyX.bin is corrupt!");
                FileClose();
                return 1;
            }
            FileClose();
            setup_aeskeyX(0x25, slot0x25KeyX);
        } else {
            Debug("Warning: Need slot0x25KeyX.bin on O3DS < 7.x");
        }
    }
    
    // check / setup seed crypto
    if (usesSeedCrypto) {
        if (FileOpen("seeddb.bin")) {
            SeedInfoEntry* entry = (SeedInfoEntry*) buffer;
            u32 found = 0;
            for (u32 i = 0x10;; i += 0x20) {
                if (FileRead(entry, 0x20, i) != 0x20)
                    break;
                if (entry->titleId == ncch->partitionId) {
                    u8 keydata[32];
                    memcpy(keydata, ncch->signature, 16);
                    memcpy(keydata + 16, entry->external_seed, 16);
                    u8 sha256sum[32];
                    sha256_context shactx;
                    sha256_starts(&shactx);
                    sha256_update(&shactx, keydata, 32);
                    sha256_finish(&shactx, sha256sum);
                    memcpy(seedKeyY, sha256sum, 16);
                    found = 1;
                }
            }
            FileClose();
            if (!found) {
                Debug("Seed not found in seeddb.bin!");
                return 1;
            }
        } else {
            Debug("Need seeddb.bin to decrypt!");
            return 1;
        }
    }
    
    // basic setup of CryptBufferInfo structs
    memset(info0.CTR, 0x00, 16);
    if (ncch->version == 1) {
        memcpy(info0.CTR, &(ncch->partitionId), 8);
    } else {
        for (u32 i = 0; i < 8; i++)
            info0.CTR[i] = ((u8*) &(ncch->partitionId))[7-i];
    }
    memcpy(info1.CTR, info0.CTR, 8);
    memcpy(info0.keyY, ncch->signature, 16);
    memcpy(info1.keyY, (usesSeedCrypto) ? seedKeyY : ncch->signature, 16);
    info1.keyslot = (usesSec3Crypto) ? 0x18 : ((uses7xCrypto) ? 0x25 : 0x2C);
    
    Debug("Decrypt ExHdr/ExeFS/RomFS (%ukB/%ukB/%uMB)",
        (ncch->size_exthdr > 0) ? 0x800 / 1024 : 0,
        (ncch->size_exefs * 0x200) / 1024,
        (ncch->size_romfs * 0x200) / (1024*1024));
        
    // process ExHeader
    if (ncch->size_exthdr > 0) {
        // Debug("Decrypting ExtHeader (%ub)...", 0x800);
        memset(info0.CTR + 12, 0x00, 4);
        if (ncch->version == 1)
            add_ctr(info0.CTR, 0x200); // exHeader offset
        else
            info0.CTR[8] = 1;
        result |= DecryptSdToSd(filename, offset + 0x200, 0x800, &info0);
    }
    
    // process ExeFS
    if (ncch->size_exefs > 0) {
        u32 offset_byte = ncch->offset_exefs * 0x200;
        u32 size_byte = ncch->size_exefs * 0x200;
        // Debug("Decrypting ExeFS (%ukB)...", size_byte / 1024);
        memset(info0.CTR + 12, 0x00, 4);
        if (ncch->version == 1)
            add_ctr(info0.CTR, offset_byte);
        else
            info0.CTR[8] = 2;
        if (uses7xCrypto || usesSeedCrypto) {
            u32 offset_code = 0;
            u32 size_code = 0;
            // find .code offset and size
            result |= DecryptSdToSd(filename, offset + offset_byte, 0x200, &info0);
            if(!FileOpen(filename))
                return 1;
            if(!DebugFileRead(buffer, 0x200, offset + offset_byte)) {
                FileClose();
                return 1;
            }
            FileClose();
            for (u32 i = 0; i < 10; i++) {
                if(memcmp(buffer + (i*0x10), ".code", 5) == 0) {
                    offset_code = *((u32*) (buffer + (i*0x10) + 0x8)) + 0x200;
                    size_code = *((u32*) (buffer + (i*0x10) + 0xC));
                    break;
                }
            }
            // special ExeFS decryption routine (only .code has new encryption)
            if (size_code > 0) {
                result |= DecryptSdToSd(filename, offset + offset_byte + 0x200, offset_code - 0x200, &info0);
                memcpy(info1.CTR, info0.CTR, 16); // this depends on the exeFS file offsets being aligned (which they are)
                add_ctr(info0.CTR, size_code / 0x10);
                info0.setKeyY = info1.setKeyY = 1;
                result |= DecryptSdToSd(filename, offset + offset_byte + offset_code, size_code, &info1);
                result |= DecryptSdToSd(filename,
                    offset + offset_byte + offset_code + size_code,
                    size_byte - (offset_code + size_code), &info0);
            } else {
                result |= DecryptSdToSd(filename, offset + offset_byte + 0x200, size_byte - 0x200, &info0);
            }
        } else {
            result |= DecryptSdToSd(filename, offset + offset_byte, size_byte, &info0);
        }
    }
    
    // process RomFS
    if (ncch->size_romfs > 0) {
        u32 offset_byte = ncch->offset_romfs * 0x200;
        u32 size_byte = ncch->size_romfs * 0x200;
        // Debug("Decrypting RomFS (%uMB)...", size_byte / (1024 * 1024));
        memset(info1.CTR + 12, 0x00, 4);
        if (ncch->version == 1)
            add_ctr(info1.CTR, offset_byte);
        else
            info1.CTR[8] = 3;
        info1.setKeyY = 1;
        result |= DecryptSdToSd(filename, offset + offset_byte, size_byte, &info1);
    }
    
    // set NCCH header flags
    ncch->flags[3] = 0x00;
    ncch->flags[7] &= 0x20^0xFF;
    ncch->flags[7] |= 0x04;
    
    // write header back
    if (!FileOpen(filename))
        return 1;
    if (!DebugFileWrite((void*) ncch, 0x200, offset)) {
        FileClose();
        return 1;
    }
    FileClose();
    
    // verify decryption
    if (result == 0) {
        char* status_str[3] = { "OK", "Fail", "-" }; 
        u32 ver_exthdr = 2;
        u32 ver_exefs = 2;
        u32 ver_romfs = 2;
        
        if (ncch->size_exthdr > 0)
            ver_exthdr = CheckHash(filename, offset + 0x200, 0x400, ncch->hash_exthdr);
        if (ncch->size_exefs_hash > 0)
            ver_exefs = CheckHash(filename, offset + (ncch->offset_exefs * 0x200), ncch->size_exefs_hash * 0x200, ncch->hash_exefs);
        if (ncch->size_romfs_hash > 0)
            ver_romfs = CheckHash(filename, offset + (ncch->offset_romfs * 0x200), ncch->size_romfs_hash * 0x200, ncch->hash_romfs);
        
        Debug("Verify ExHdr/ExeFS/RomFS: %s/%s/%s", status_str[ver_exthdr], status_str[ver_exefs], status_str[ver_romfs]);
        result = (((ver_exthdr | ver_exefs | ver_romfs) & 1) == 0) ? 0 : 1;
    }
    
    
    return result;
}

u32 DecryptNcsdNcchBatch()
{
    const char* ncsd_partition_name[8] = {
        "Executable", "Manual", "DPC", "Unknown", "Unknown", "Unknown", "UpdateN3DS", "UpdateO3DS" 
    };
    u8* buffer = (u8*) 0x20316000;
    u32 n_processed = 0;
    u32 n_failed = 0;
    
    if (!DebugDirOpen(DECRYPT_DIR)) {
        Debug("Files to decrypt go to %s/!", DECRYPT_DIR);
        return 1;
    }
    
    char path[256];
    u32 path_len = strnlen(DECRYPT_DIR, 128);
    memcpy(path, DECRYPT_DIR, path_len);
    path[path_len++] = '/';
    
    while (DirRead(path + path_len, 256 - path_len)) {
        if (!FileOpen(path))
            continue;
        if (!FileRead(buffer, 0x200, 0x0)) {
            FileClose();
            continue;
        }
        FileClose();
        
        if (memcmp(buffer + 0x100, "NCCH", 4) == 0) {
            Debug("Decrypting NCCH \"%s\"", path + path_len);
            if (DecryptNcch(path, 0x00) == 0) {
                Debug("Success!");
                n_processed++;
            } else {
                Debug("Failed!");
                n_failed++;
            }
        } else if (memcmp(buffer + 0x100, "NCSD", 4) == 0) {
            Debug("Decrypting NCSD \"%s\"", path + path_len);
            u32 p;
            for (p = 0; p < 8; p++) {
                u32 offset = *((u32*) (buffer + 0x120 + (p*0x8))) * 0x200;
                u32 size = *((u32*) (buffer + 0x124 + (p*0x8))) * 0x200;
                if (size > 0) {
                    Debug("Partition %i (%s)", p, ncsd_partition_name[p]);
                    if (DecryptNcch(path, offset) != 0)
                        break;
                }
            }
            if ( p == 8 ) {
                Debug("Success!");
                n_processed++;
            } else {
                Debug("Failed!");
                n_failed++;
            }
        }
    }
    
    DirClose();
    
    if (n_processed) {
        Debug("");
        Debug("%ux decrypted / %u failed ", n_processed, n_failed);
    }
    
    return !(n_processed);
}

u32 CtrNandPadgen()
{
    u32 keyslot;
    u32 nand_size;

    if(GetUnitPlatform() == PLATFORM_3DS) {
        keyslot = 0x4;
        nand_size = 758;
    } else {
        keyslot = 0x5;
        nand_size = 1055;
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
        
    CryptBufferInfo decryptInfo = {.keyslot = info->keyslot, .setKeyY = info->setKeyY, .mode = info->mode, .buffer = buffer};
    memcpy(decryptInfo.CTR, info->CTR, 16);
    memcpy(decryptInfo.keyY, info->keyY, 16);
    u32 size_bytes = info->size_mb * 1024*1024;
    for (u32 i = 0; i < size_bytes; i += BUFFER_MAX_SIZE) {
        u32 curr_block_size = min(BUFFER_MAX_SIZE, size_bytes - i);
        decryptInfo.size = curr_block_size;
        memset(buffer, 0x00, curr_block_size);
        ShowProgress(i, size_bytes);
        CryptBuffer(&decryptInfo);
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
    u32 nand_size = getMMCDevice(0)->total_size * 0x200;
    u32 result = 0;

    Debug("Dumping System NAND. Size (MB): %u", nand_size / (1024 * 1024));

    if (!DebugFileCreate("/NAND.bin", true))
        return 1;

    u32 n_sectors = nand_size / NAND_SECTOR_SIZE;
    for (u32 i = 0; i < n_sectors; i += SECTORS_PER_READ) {
        ShowProgress(i, n_sectors);
        ReadNandSectors(i, SECTORS_PER_READ, buffer);
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
    snprintf(filename, 32, "/%s.bin", p->name);
    
    return DecryptNandToFile(filename, p->offset, p->size, p);
}

u32 DecryptAllNandPartitions() {
    u32 result = 0;
    bool o3ds = (GetUnitPlatform() == PLATFORM_3DS);
    
    result |= DecryptNandPartition(&(partitions[0])); // TWLN
    result |= DecryptNandPartition(&(partitions[1])); // TWLP
    result |= DecryptNandPartition(&(partitions[2])); // AGBSAVE
    result |= DecryptNandPartition(&(partitions[3])); // FIRM0
    result |= DecryptNandPartition(&(partitions[4])); // FIRM1
    result |= DecryptNandPartition(&(partitions[(o3ds) ? 5 : 6])); // CTRNAND O3DS / N3DS

    return result;
}

u32 DecryptTwlNandPartition() {
    return DecryptNandPartition(&(partitions[0])); // TWLN
}
    
u32 DecryptCtrNandPartition() {
    bool o3ds = (GetUnitPlatform() == PLATFORM_3DS);
    return DecryptNandPartition(&(partitions[(o3ds) ? 5 : 6])); // CTRNAND O3DS / N3DS
}

u32 EncryptMemToNand(u8* buffer, u32 offset, u32 size, PartitionInfo* partition)
{
    CryptBufferInfo info = {.keyslot = partition->keyslot, .setKeyY = 0, .size = size, .buffer = buffer, .mode = partition->mode};
    if(GetNandCtr(info.CTR, offset) != 0)
        return 1;

    u32 n_sectors = (size + NAND_SECTOR_SIZE - 1) / NAND_SECTOR_SIZE;
    u32 start_sector = offset / NAND_SECTOR_SIZE;
    CryptBuffer(&info);
    WriteNandSectors(start_sector, n_sectors, buffer);

    return 0;
}

u32 EncryptFileToNand(const char* filename, u32 offset, u32 size, PartitionInfo* partition)
{
    u8* buffer = BUFFER_ADDRESS;
    u32 result = 0;

    if (!DebugFileOpen(filename))
        return 1;
    
    if (FileGetSize() != size) {
        Debug("%s has wrong size", filename);
        FileClose();
        return 1;
    }

    for (u32 i = 0; i < size; i += NAND_SECTOR_SIZE * SECTORS_PER_READ) {
        u32 read_bytes = min(NAND_SECTOR_SIZE * SECTORS_PER_READ, (size - i));
        ShowProgress(i, size);
        if(!DebugFileRead(buffer, read_bytes, i)) {
            result = 1;
            break;
        }
        EncryptMemToNand(buffer, offset + i, read_bytes, partition);
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}

u32 RestoreNand()
{
    u8* buffer = BUFFER_ADDRESS;
    u32 nand_size = getMMCDevice(0)->total_size * 0x200;
    u32 result = 0;

    if (!DebugFileOpen("/NAND.bin"))
        return 1;
    if (nand_size != FileGetSize()) {
        FileClose();
        Debug("NAND backup has the wrong size!");
        return 1;
    };
    
    Debug("Restoring System NAND. Size (MB): %u", nand_size / (1024 * 1024));

    u32 n_sectors = nand_size / NAND_SECTOR_SIZE;
    for (u32 i = 0; i < n_sectors; i += SECTORS_PER_READ) {
        ShowProgress(i, n_sectors);
        if(!DebugFileRead(buffer, NAND_SECTOR_SIZE * SECTORS_PER_READ, i * NAND_SECTOR_SIZE)) {
            result = 1;
            break;
        }
        WriteNandSectors(i, SECTORS_PER_READ, buffer);
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}

u32 InjectNandPartition(PartitionInfo* p) {
    char filename[32];
    u8 magic[NAND_SECTOR_SIZE];
    
    // File check
    snprintf(filename, 32, "/%s.bin", p->name);
    if (FileOpen(filename)) {
        FileClose();
    } else {
        return 1;
    }
    
    Debug("Encrypting & Injecting %s, size (MB): %u", p->name, p->size / (1024 * 1024));
    
    // Encryption check
    if (DecryptNandToMem(magic, p->offset, 16, p) != 0)
        return 1;
    if ((p->magic[0] != 0xFF) && (memcmp(p->magic, magic, 8) != 0)) {
        Debug("Decryption error, please contact us");
        return 1;
    }
    
    // File check
    if (FileOpen(filename)) {
        if(!DebugFileRead(magic, 8, 0)) {
            FileClose();
            return 1;
        }
        if ((p->magic[0] != 0xFF) && (memcmp(p->magic, magic, 8) != 0)) {
            Debug("Bad file content, won't inject");
            FileClose();
            return 1;
        }
        FileClose();
    }
    
    return EncryptFileToNand(filename, p->offset, p->size, p);
}

u32 InjectAllNandPartitions() {
    u32 result = 1;
    bool o3ds = (GetUnitPlatform() == PLATFORM_3DS);
    
    result &= InjectNandPartition(&(partitions[0])); // TWLN
    result &= InjectNandPartition(&(partitions[1])); // TWLP
    result &= InjectNandPartition(&(partitions[2])); // AGBSAVE
    result &= InjectNandPartition(&(partitions[3])); // FIRM0
    result &= InjectNandPartition(&(partitions[4])); // FIRM1
    result &= InjectNandPartition(&(partitions[(o3ds) ? 5 : 6])); // CTRNAND O3DS / N3DS
    
    return result;
}

u32 InjectTwlNandPartition() {
    return InjectNandPartition(&(partitions[0])); // TWLN
}

u32 InjectCtrNandPartition() {
    bool o3ds = (GetUnitPlatform() == PLATFORM_3DS);
    return InjectNandPartition(&(partitions[(o3ds) ? 5 : 6])); // CTRNAND O3DS / N3DS
}
