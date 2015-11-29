#include <string.h>
#include <stdio.h>

#include "fs.h"
#include "draw.h"
#include "platform.h"
#include "decryptor/features.h"
#include "decryptor/crypto.h"
#include "decryptor/sha256.h"
#include "decryptor/decryptor.h"
#include "decryptor/nand.h"
#include "decryptor/nandfat.h"
#include "decryptor/game.h"


u32 NcchPadgen(u32 param)
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

u32 SdPadgen(u32 param)
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

u32 UpdateSeedDb(u32 param)
{
    PartitionInfo* ctrnand_info = GetPartitionInfo(P_CTRNAND);
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

u32 DecryptNcsdNcchBatch(u32 param)
{
    const char* ncsd_partition_name[8] = {
        "Executable", "Manual", "DPC", "Unknown", "Unknown", "Unknown", "UpdateN3DS", "UpdateO3DS" 
    };
    char* batch_dir = GAME_DIR;
    u8* buffer = (u8*) 0x20316000;
    u32 n_processed = 0;
    u32 n_failed = 0;
    
    if (!DebugDirOpen(batch_dir)) {
        #ifdef WORK_DIR
        Debug("Trying %s/ instead...", WORK_DIR);
        if (!DebugDirOpen(WORK_DIR)) {
            Debug("No working directory found!");
            return 1;
        }
        batch_dir = WORK_DIR;
        #else
        Debug("Files to process go to %s/!", batch_dir);
        return 1;
        #endif
    }
    
    char path[256];
    u32 path_len = strnlen(batch_dir, 128);
    memcpy(path, batch_dir, path_len);
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
                if (size == 0) 
                    continue;
                Debug("Partition %i (%s)", p, ncsd_partition_name[p]);
                if (DecryptNcch(path, offset) != 0)
                    break;
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
