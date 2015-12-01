#include "fs.h"
#include "draw.h"
#include "platform.h"
#include "decryptor/crypto.h"
#include "decryptor/decryptor.h"
#include "decryptor/nand.h"
#include "decryptor/nandfat.h"
#include "decryptor/game.h"


u32 GetSdCtr(u8* ctr, const char* path)
{
    // get AES counter
    u8 hashstr[256];
    u8 sha256sum[32];
    u32 plen = 0;
    for (u32 i = 0; i < 128; i++) {
        hashstr[2*i] = path[i];
        hashstr[2*i+1] = 0;
        if (path[i] == 0) {
            plen = i;
            break;
        }
    }
    sha_init(SHA256_MODE);
    sha_update(hashstr, (plen + 1) * 2);
    sha_get(sha256sum);
    for (u32 i = 0; i < 16; i++)
        ctr[i] = sha256sum[i] ^ sha256sum[i+16];
    
    return 0;
}

u32 SdInfoGen(SdInfo* info)
{
    char* filelist = (char*)0x20400000;
    
    Debug("Generating SDinfo.bin in memory...");
    
    if (!GetFileList("/Nintendo 3DS", filelist, 0x100000, true)) {
        Debug("Failed retrieving the filelist");
    }
    
    u32 n = 0;
    SdInfoEntry* entries = info->entries;
    for (char* path = strtok(filelist, "\n"); path != NULL; path = strtok(NULL, "\n")) {
        u32 plen = strnlen(path, 256);
        if ((strncmp(path, "/Nintendo 3DS/Private/", 22) == 0) || (plen < 13 + 33 + 33))
            continue;
        // get size in MB
        if (!FileOpen(path))
            continue;
        entries[n].size_mb = (FileGetSize() + (1024 * 1024) - 1) / (1024 * 1024);
        FileClose();
        // skip to relevant part of path
        path += 13 + 33 + 33;
        plen -= 13 + 33 + 33;
        if ((path[0] != '/') || (plen >= 128)) continue;
        // get filename
        char* filename = entries[n].filename;
        filename[0] = '/';
        for (u32 i = 1; i < 180 && path[i] != 0; i++)
            filename[i] = (path[i] == '/') ? '.' : path[i];
        strcpy(filename + plen, ".xorpad");
        // get AES counter
        GetSdCtr(entries[n].ctr, path);
        // duplicate check
        u32 d;
        for (d = 0; d < n; d++)
            if (strncmp(entries[n].filename, entries[d].filename, 180) == 0) break;
        if (d < n) {
            entries[d].size_mb = max(entries[d].size_mb, entries[n].size_mb);
        } else n++;
        if (n >= MAX_ENTRIES) break;
    }
    info->n_entries = n;
    
    return (n > 0) ? 0 : 1;
}

u32 NcchPadgen(u32 param)
{
    u32 result;

    NcchInfo *info = (NcchInfo*)0x20316000;
    SeedInfo *seedinfo = (SeedInfo*)0x20400000;

    if (DebugFileOpen("slot0x25KeyX.bin")) {
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

    if (DebugFileOpen("seeddb.bin")) {
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

    if (!DebugFileOpen("ncchinfo.bin"))
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
        memcpy(padInfo.ctr, info->entries[i].ctr, 16);
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
            sha_init(SHA256_MODE);
            sha_update(keydata, 32);
            sha_get(sha256sum);
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
    if (DebugFileOpen("movable.sed")) {
        if (!DebugFileRead(&movable_seed, 0x120, 0)) {
            FileClose();
            return 1;
        }
        FileClose();
        if (memcmp(movable_seed, "SEED", 4) != 0) {
            Debug("movable.sed is corrupt!");
            return 1;
        }
        setup_aeskeyY(0x34, &movable_seed[0x110]);
        use_aeskey(0x34);
    }

    if (DebugFileOpen("SDinfo.bin")) {
        if (!DebugFileRead(info, 4, 0)) {
            FileClose();
            return 1;
        }
        if (!info->n_entries || info->n_entries > MAX_ENTRIES) {
            FileClose();
            Debug("Too many/few entries!");
            return 1;
        }
        if (!DebugFileRead(info->entries, info->n_entries * sizeof(SdInfoEntry), 4)) {
            FileClose();
            return 1;
        }
        FileClose();
    } else if (SdInfoGen(info) != 0) {
        return 1;
    }

    Debug("Number of entries: %i", info->n_entries);

    for(u32 i = 0; i < info->n_entries; i++) {
        Debug ("Creating pad number: %i. Size (MB): %i", i+1, info->entries[i].size_mb);

        PadInfo padInfo = {.keyslot = 0x34, .setKeyY = 0, .size_mb = info->entries[i].size_mb, .mode = AES_CNT_CTRNAND_MODE};
        memcpy(padInfo.ctr, info->entries[i].ctr, 16);
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
    if (DebugFileOpen("seeddb.bin")) {
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
        if (!DebugFileCreate("seeddb.bin", true))
            return 1;
        memset(seedinfo, 0x00, 16);
    }
    
    // search and extract seeds
    for ( int n = 0; n < 2; n++ ) {
        u8* seed_data = buffer + ((n == 0) ? 0x7000 : 0x5C000);
        for ( size_t i = 0; i < 2000; i++ ) {
            static const u8 magic[4] = { 0x00, 0x00, 0x04, 0x00 };
            u8* titleId = seed_data + (i*8);
            u8* seed = seed_data + (2000*8) + (i*16);
            if (memcmp(titleId + 4, magic, 4) != 0) continue;
            // seed found, check if it already exists
            u32 entryPos = 0;
            for (entryPos = 0; entryPos < seedinfo->n_entries; entryPos++)
                if (memcmp(titleId, &(seedinfo->entries[entryPos].titleId), 8) == 0) break;
            if (entryPos < seedinfo->n_entries) {
                Debug("Found %08X%08X seed (duplicate)", getle32(titleId + 4), getle32(titleId));
                continue;
            }
            // seed is new, create a new entry
            Debug("Found %08X%08X seed (new)", getle32(titleId + 4), getle32(titleId));
            memset(&(seedinfo->entries[entryPos]), 0x00, sizeof(SeedInfoEntry));
            memcpy(&(seedinfo->entries[entryPos].titleId), titleId, 8);
            memcpy(&(seedinfo->entries[entryPos].external_seed), seed, 16);
            seedinfo->n_entries++;
            nNewSeeds++;
        }
    }
    
    if (nNewSeeds == 0) {
        Debug("Found no new seeds, %i total", seedinfo->n_entries);
        FileClose();
        return 0;
    }
    
    Debug("Found %i new seeds, %i total", nNewSeeds, seedinfo->n_entries);
    if (!DebugFileWrite(seedinfo, 16 + seedinfo->n_entries * sizeof(SeedInfoEntry), 0))
        return 1;
    FileClose();
    
    return 0;
}

u32 CryptSdToSd(const char* filename, u32 offset, u32 size, CryptBufferInfo* info)
{
    u8* buffer = BUFFER_ADDRESS;
    u32 offset_16 = offset % 16;
    u32 result = 0;

    // no DebugFileOpen() - at this point the file has already been checked enough
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
    
    if (!FileOpen(filename))
        return 1;
    sha_init(SHA256_MODE);
    for (u32 i = 0; i < size; i += BUFFER_MAX_SIZE) {
        u32 read_bytes = min(BUFFER_MAX_SIZE, (size - i));
        if(!FileRead(buffer, read_bytes, offset + i)) {
            FileClose();
            return 1;
        }
        sha_update(buffer, read_bytes);
    }
    sha_get(hash);
    FileClose();
    
    return (memcmp(hash, digest, 32) == 0) ? 0 : 1; 
}

u32 CryptNcch(const char* filename, u32 offset, u32 size, u64 seedId, u8* encrypt_flags)
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
 
    // check (again) for magic number
    if (memcmp(ncch->magic, "NCCH", 4) != 0) {
        Debug("Not a NCCH container");
        return 2; // not an actual error
    }
    
    // size plausibility check
    u32 size_sum = 0x200 + ((ncch->size_exthdr) ? 0x800 : 0x0) + 0x200 *
        (ncch->size_plain + ncch->size_logo + ncch->size_exefs + ncch->size_romfs);
    if (ncch->size * 0x200 < size_sum) {
        Debug("Probably not a NCCH container");
        return 2; // not an actual error
    }        
    
    // check if encrypted
    if (!encrypt_flags && (ncch->flags[7] & 0x04)) {
        Debug("NCCH is not encrypted");
        return 2; // not an actual error
    } else if (encrypt_flags && !(ncch->flags[7] & 0x04)) {
        Debug("NCCH is already encrypted");
        return 2; // not an actual error
    } else if (encrypt_flags && (encrypt_flags[7] & 0x04)) {
        Debug("Nothing to do!");
        return 2; // not an actual error
    }
    
    // check size
    if ((size > 0) && (ncch->size * 0x200 > size)) {
        Debug("NCCH size is out of bounds");
        return 1;
    }
    
    // select correct title ID for seed crypto
    if (seedId == 0) seedId = ncch->partitionId;
    
    // copy over encryption parameters (if applicable)
    if (encrypt_flags) {
        ncch->flags[3] = encrypt_flags[3];
        ncch->flags[7] &= (0x01|0x20|0x04)^0xFF;
        ncch->flags[7] |= (0x01|0x20)&encrypt_flags[7];
    }
    
    // check crypto type
    bool uses7xCrypto = ncch->flags[3];
    bool usesSeedCrypto = ncch->flags[7] & 0x20;
    bool usesSec3Crypto = (ncch->flags[3] == 0x0A);
    bool usesSec4Crypto = (ncch->flags[3] == 0x0B);
    bool usesFixedKey = ncch->flags[7] & 0x01;
    
    Debug("Code / Crypto: %s / %s%s%s%s", ncch->productCode, (usesFixedKey) ? "FixedKey " : "", (usesSec4Crypto) ? "Secure4 " : (usesSec3Crypto) ? "Secure3 " : (uses7xCrypto) ? "7x " : "", (usesSeedCrypto) ? "Seed " : "", (!uses7xCrypto && !usesSeedCrypto && !usesFixedKey) ? "Standard" : "");
    
    // setup zero key crypto
    if (usesFixedKey) {
        // from https://github.com/profi200/Project_CTR/blob/master/makerom/pki/dev.h
        u8 zeroKey[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        u8 sysKey[16]  = {0x52, 0x7C, 0xE6, 0x30, 0xA9, 0xCA, 0x30, 0x5F, 0x36, 0x96, 0xF3, 0xCD, 0xE9, 0x54, 0x19, 0x4B};
        if (uses7xCrypto || usesSeedCrypto) {
            Debug("Crypto combination is not allowed!");
            return 1;
        }
        info1.setKeyY = info0.setKeyY = 0;
        info1.keyslot = info0.keyslot = 0x11;
        setup_aeskey(0x11, (ncch->programId & ((u64) 0x10 << 32)) ? sysKey : zeroKey);
    }
    
    // check secure4 crypto
    if (usesSec4Crypto) {
        Debug("Secure4 crypto is not supported!");
        return 1;
    }
    
    // check / setup 7x crypto
    if (uses7xCrypto && (GetUnitPlatform() == PLATFORM_3DS)) {
        if (usesSec3Crypto) {
            Debug("Secure3 crypto needs a N3DS!");
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
                if (entry->titleId == seedId) {
                    u8 keydata[32];
                    memcpy(keydata, ncch->signature, 16);
                    memcpy(keydata + 16, entry->external_seed, 16);
                    u8 sha256sum[32];
                    sha_init(SHA256_MODE);
                    sha_update(keydata, 32);
                    sha_get(sha256sum);
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
            Debug("Need seeddb.bin for seed crypto!");
            return 1;
        }
    }
    
    // basic setup of CryptBufferInfo structs
    memset(info0.ctr, 0x00, 16);
    if (ncch->version == 1) {
        memcpy(info0.ctr, &(ncch->partitionId), 8);
    } else {
        for (u32 i = 0; i < 8; i++)
            info0.ctr[i] = ((u8*) &(ncch->partitionId))[7-i];
    }
    memcpy(info1.ctr, info0.ctr, 8);
    memcpy(info0.keyY, ncch->signature, 16);
    memcpy(info1.keyY, (usesSeedCrypto) ? seedKeyY : ncch->signature, 16);
    info1.keyslot = (usesSec3Crypto) ? 0x18 : ((uses7xCrypto) ? 0x25 : 0x2C);
    
    Debug("%s ExHdr/ExeFS/RomFS (%ukB/%ukB/%uMB)",
        (encrypt_flags) ? "Encrypt" : "Decrypt",
        (ncch->size_exthdr > 0) ? 0x800 / 1024 : 0,
        (ncch->size_exefs * 0x200) / 1024,
        (ncch->size_romfs * 0x200) / (1024*1024));
        
    // process ExHeader
    if (ncch->size_exthdr > 0) {
        memset(info0.ctr + 12, 0x00, 4);
        if (ncch->version == 1)
            add_ctr(info0.ctr, 0x200); // exHeader offset
        else
            info0.ctr[8] = 1;
        result |= CryptSdToSd(filename, offset + 0x200, 0x800, &info0);
    }
    
    // process ExeFS
    if (ncch->size_exefs > 0) {
        u32 offset_byte = ncch->offset_exefs * 0x200;
        u32 size_byte = ncch->size_exefs * 0x200;
        memset(info0.ctr + 12, 0x00, 4);
        if (ncch->version == 1)
            add_ctr(info0.ctr, offset_byte);
        else
            info0.ctr[8] = 2;
        if (uses7xCrypto || usesSeedCrypto) {
            u32 offset_code = 0;
            u32 size_code = 0;
            // find .code offset and size
            if (!encrypt_flags) // decrypt this first (when decrypting)
                result |= CryptSdToSd(filename, offset + offset_byte, 0x200, &info0);
            if(!FileOpen(filename))
                return 1;
            if(!DebugFileRead(buffer, 0x200, offset + offset_byte)) {
                FileClose();
                return 1;
            }
            FileClose();
            for (u32 i = 0; i < 10; i++) {
                if(memcmp(buffer + (i*0x10), ".code", 5) == 0) {
                    offset_code = getle32(buffer + (i*0x10) + 0x8) + 0x200;
                    size_code = getle32(buffer + (i*0x10) + 0xC);
                    break;
                }
            }
            if (encrypt_flags) // encrypt this last (when encrypting)
                result |= CryptSdToSd(filename, offset + offset_byte, 0x200, &info0);
            // special ExeFS decryption routine (only .code has new encryption)
            if (size_code > 0) {
                result |= CryptSdToSd(filename, offset + offset_byte + 0x200, offset_code - 0x200, &info0);
                memcpy(info1.ctr, info0.ctr, 16); // this depends on the exeFS file offsets being aligned (which they are)
                add_ctr(info0.ctr, size_code / 0x10);
                info0.setKeyY = info1.setKeyY = 1;
                result |= CryptSdToSd(filename, offset + offset_byte + offset_code, size_code, &info1);
                result |= CryptSdToSd(filename,
                    offset + offset_byte + offset_code + size_code,
                    size_byte - (offset_code + size_code), &info0);
            } else {
                result |= CryptSdToSd(filename, offset + offset_byte + 0x200, size_byte - 0x200, &info0);
            }
        } else {
            result |= CryptSdToSd(filename, offset + offset_byte, size_byte, &info0);
        }
    }
    
    // process RomFS
    if (ncch->size_romfs > 0) {
        u32 offset_byte = ncch->offset_romfs * 0x200;
        u32 size_byte = ncch->size_romfs * 0x200;
        memset(info1.ctr + 12, 0x00, 4);
        if (ncch->version == 1)
            add_ctr(info1.ctr, offset_byte);
        else
            info1.ctr[8] = 3;
        info1.setKeyY = 1;
        result |= CryptSdToSd(filename, offset + offset_byte, size_byte, &info1);
    }
    
    // set NCCH header flags
    if (!encrypt_flags) {
        ncch->flags[3] = 0x00;
        ncch->flags[7] &= (0x01|0x20)^0xFF;
        ncch->flags[7] |= 0x04;
    }
    
    // write header back
    if (!FileOpen(filename))
        return 1;
    if (!DebugFileWrite((void*) ncch, 0x200, offset)) {
        FileClose();
        return 1;
    }
    FileClose();
    
    // verify decryption
    if ((result == 0) && !encrypt_flags) {
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

u32 CryptGameFiles(u32 param)
{
    u8 ncch_crypt_none[8]     = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04 };
    u8 ncch_crypt_standard[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const char* ncsd_partition_name[8] = {
        "Executable", "Manual", "DPC", "Unknown", "Unknown", "Unknown", "UpdateN3DS", "UpdateO3DS" 
    };
    char* batch_dir = GAME_DIR;
    u8* buffer = (u8*) 0x20316000;
    u8* ncch_crypt = (param & GC_NCCH_ENCRYPT) ? ncch_crypt_standard : NULL;

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
            Debug("Processing NCCH \"%s\"", path + path_len);
            if (CryptNcch(path, 0x00, 0, 0, ncch_crypt) != 1) {
                Debug("Success!");
                n_processed++;
            } else {
                Debug("Failed!");
                n_failed++;
            }
        } else if (memcmp(buffer + 0x100, "NCSD", 4) == 0) {
            if (getle64(buffer + 0x110) != 0) 
                continue; // skip NAND backup NCSDs
            Debug("Processing NCSD \"%s\"", path + path_len);
            u32 p;
            for (p = 0; p < 8; p++) {
                u64 seedId = (p) ? getle64(buffer + 0x108) : 0;
                u32 offset = getle32(buffer + 0x120 + (p*0x8)) * 0x200;
                u32 size = getle32(buffer + 0x124 + (p*0x8)) * 0x200;
                if (size == 0) 
                    continue;
                Debug("Partition %i (%s)", p, ncsd_partition_name[p]);
                if (CryptNcch(path, offset, size, seedId, ncch_crypt) == 1)
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
        Debug("%ux processed / %ux failed ", n_processed, n_failed);
    } else if (!n_failed) {
        Debug("Nothing found in %s/!", batch_dir);
    }
    
    return !n_processed;
}

u32 CryptSdFiles(u32 param) {
    const char* subpaths[] = {"dbs", "extdata", "title", NULL};
    char* batch_dir = GAME_DIR;
    u32 n_processed = 0;
    u32 n_failed = 0;
    u32 plen = 0;
    
    if (!DebugDirOpen(batch_dir)) {
        #ifdef WORK_DIR
        Debug("Trying %s/ instead...", WORK_DIR);
        if (!DebugDirOpen(WORK_DIR)) {
            Debug("No working directory found!");
            return 1;
        }
        batch_dir = WORK_DIR;
        #else
        Debug("Folders to decrypt go to %s/!", batch_dir);
        return 1;
        #endif
    }
    DirClose();
    plen = strnlen(batch_dir, 128);
    
    // Load console 0x34 keyY from movable.sed if present on SD card
    if (DebugFileOpen("movable.sed")) {
        u8 movable_keyY[16];
        u8 magic[4];
        if (!DebugFileRead(magic, 4, 0)) {
            FileClose();
            return 1;
        }
        if (memcmp(magic, "SEED", 4) != 0) {
            FileClose();
            Debug("movable.sed is corrupt!");
            return 1;
        }
        if (!DebugFileRead(movable_keyY, 0x10, 0x110)) {
            FileClose();
            return 1;
        }
        FileClose();
        setup_aeskeyY(0x34, movable_keyY);
        use_aeskey(0x34);
    }
    
    // main processing loop
    for (u32 s = 0; subpaths[s] != NULL; s++) {
        char* filelist = (char*) 0x20400000;
        char basepath[128];
        u32 bplen;
        Debug("Processing subpath \"%s\"...", subpaths[s]);
        sprintf(basepath, "%s/%s", batch_dir, subpaths[s]);
        if (!GetFileList(basepath, filelist, 0x100000, true)) {
            Debug("Not found!");
            continue;
        }
        bplen = strnlen(basepath, 128);
        for (char* path = strtok(filelist, "\n"); path != NULL; path = strtok(NULL, "\n")) {
            u32 fsize = 0;
            CryptBufferInfo info = {.keyslot = 0x34, .setKeyY = 0, .mode = AES_CNT_CTRNAND_MODE};
            GetSdCtr(info.ctr, path + plen);
            if (FileOpen(path)) {
                fsize = FileGetSize();
                FileClose();
            } else {
                Debug("Could not open: %s", path + bplen);
                n_failed++;
                continue;
            }
            Debug("%2u: %s", n_processed, path + bplen);
            if (CryptSdToSd(path, 0, fsize, &info) == 0) {
                n_processed++;
            } else {
                Debug("Failed!");
                n_failed++;
            }
        }
    }
    
    return 0;
}
