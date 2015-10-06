#include <string.h>
#include <stdio.h>

#include "fs.h"
#include "draw.h"
#include "platform.h"
#include "decryptor/features.h"
#include "decryptor/crypto.h"
#include "decryptor/decryptor.h"
#include "decryptor/nand.h"
#include "fatfs/sdmmc.h"

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


u32 SetNand(bool use_emunand)
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

bool IsEmuNand()
{
    return emunand_header;
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

PartitionInfo* GetNandPartition(u32 partition_id)
{
    if (partition_id == P_CTRNAND)
        return &(partitions[(GetUnitPlatform() == PLATFORM_3DS) ? 5 : 6]);
    
    return &(partitions[partition_id]);
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

u32 DumpTicket() {
    PartitionInfo* ctrnand_info = GetNandPartition(P_CTRNAND);
    u32 offset;
    u32 size;
    
    Debug("Searching for ticket.db...");
    if (SeekFileInNand(&offset, &size, "DBS        TICKET  DB ", ctrnand_info) != 0) {
        Debug("Failed!");
        return 1;
    }
    Debug("Found at %08X, size %uMB", offset, size / (1024 * 1024));
    
    if (DecryptNandToFile((IsEmuNand()) ? "/ticket_emu.db" : "/ticket.db", offset, size, ctrnand_info) != 0)
        return 1;
    
    return 0;
}

u32 DumpSeedsave()
{
    PartitionInfo* ctrnand_info = GetNandPartition(P_CTRNAND);
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
    
    for (u32 partition_id = 0; partition_id < 6; partition_id++)
        result |= DecryptNandPartition(GetNandPartition(partition_id));
    
    return result;
}

u32 DecryptTwlNandPartition() {
    return DecryptNandPartition(GetNandPartition(P_TWLN)); // TWLN
}
    
u32 DecryptCtrNandPartition() {
    return DecryptNandPartition(GetNandPartition(P_CTRNAND)); // CTRNAND O3DS / N3DS
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
    
    for (u32 partition_id = 0; partition_id < 6; partition_id++)
        result &= InjectNandPartition(GetNandPartition(partition_id));
    
    return result;
}

u32 InjectTwlNandPartition() {
    return InjectNandPartition(GetNandPartition(P_TWLN)); // TWLN
}

u32 InjectCtrNandPartition() {
    return InjectNandPartition(GetNandPartition(P_CTRNAND)); // CTRNAND O3DS / N3DS
}
