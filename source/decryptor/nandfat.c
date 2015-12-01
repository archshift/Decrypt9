#include "fs.h"
#include "draw.h"
#include "decryptor/decryptor.h"
#include "decryptor/nand.h"
#include "decryptor/nandfat.h"

NandFileInfo fileList[] = {
    { "ticket.db",             "ticket_emu.db",         "DBS        TICKET  DB ",                  P_CTRNAND },
    { "title.db",              "title_emu.db",          "DBS        TITLE   DB ",                  P_CTRNAND },
    { "import.db",             "import_emu.db",         "DBS        IMPORT  DB ",                  P_CTRNAND },
    { "certs.db",              "certs_emu.db",          "DBS        CERTS   DB ",                  P_CTRNAND },
    { "SecureInfo_A",          "SecureInfo_A",          "RW         SYS        SECURE~?   ",       P_CTRNAND },
    { "LocalFriendCodeSeed_B", "LocalFriendCodeSeed_B", "RW         SYS        LOCALF~?   ",       P_CTRNAND },
    { "rand_seed",             "rand_seed",             "RW         SYS        RAND_S~?   ",       P_CTRNAND },
    { "movable.sed",           "movable.sed",           "PRIVATE    MOVABLE SED",                  P_CTRNAND },
    { "seedsave.bin", "seedsave.bin", "DATA       ???????????SYSDATA    0001000F   00000000   ",   P_CTRNAND }
};


NandFileInfo* GetNandFileInfo(u32 file_id)
{
    u32 f = 0;
    for(; !(file_id & (1<<f)) && (f < 32); f++);
    return (f >= 32) ? NULL : &(fileList[f]);
}

u32 SeekFileInNand(u32* offset, u32* size, const char* path, PartitionInfo* partition)
{
    // poor mans NAND FAT file seeker:
    // - path must be in FAT 8+3 format, without dots or slashes
    //   example: DIR1_______DIR2_______FILENAMEEXT
    // - can't handle long filenames
    // - dirs must not exceed 1024 entries
    // - fragmentation not supported
    
    u8* buffer = BUFFER_ADDRESS;
    u32 p_size = partition->size;
    u32 p_offset = partition->offset;
    u32 fat_pos = 0;
    bool found = false;
    
    if (strnlen(path, 256) % (8+3) != 0)
        return 1;
    
    DecryptNandToMem(buffer, p_offset, NAND_SECTOR_SIZE, partition);
    
    // good FAT header description found here: http://www.compuphase.com/mbr_fat.htm
    u32 fat_start = NAND_SECTOR_SIZE * getle16(buffer + 0x0E);
    u32 fat_count = buffer[0x10];
    u32 fat_size = NAND_SECTOR_SIZE * getle16(buffer + 0x16) * fat_count;
    u32 root_size = getle16(buffer + 0x11) * 0x20;
    u32 cluster_start = fat_start + fat_size + root_size;
    u32 cluster_size = buffer[0x0D] * NAND_SECTOR_SIZE;
    
    for (*offset = p_offset + fat_start + fat_size; strnlen(path, 256) >= 8+3; path += 8+3) {
        if (*offset - p_offset > p_size)
            return 1;
        found = false;
        DecryptNandToMem(buffer, *offset, cluster_size, partition);
        for (u32 i = 0x00; i < cluster_size; i += 0x20) {
            const static char zeroes[8+3] = { 0x00 };
            // skip invisible, deleted and lfn entries
            if ((buffer[i] == '.') || (buffer[i] == 0xE5) || (buffer[i+0x0B] == 0x0F))
                continue;
            else if (memcmp(buffer + i, zeroes, 8+3) == 0)
                return 1;
            u32 p; // search for path in fat folder structure, accept '?' wildcards
            for (p = 0; (p < 8+3) && (path[p] == '?' || buffer[i+p] == path[p]); p++);
            if (p != 8+3) continue;
            // entry found, store offset and move on
            fat_pos = getle16(buffer + i + 0x1A);
            *offset = p_offset + cluster_start + (fat_pos - 2) * cluster_size;
            *size = getle32(buffer + i + 0x1C);
            found = true;
            break;
        }
        if (!found) break;
    }
    
    // check for fragmentation
    if (found && (*size > cluster_size)) {  
        if (fat_size / fat_count > 0x100000) // prevent buffer overflow
            return 1; // fishy FAT table size - should never happen
        DecryptNandToMem(buffer, p_offset + fat_start, fat_size / fat_count, partition);
        for (u32 i = 0; i < (*size - 1) / cluster_size; i++) {
            if (*(((u16*) buffer) + fat_pos + i) != fat_pos + i + 1)
                return 1;
        } // no need to check the files last FAT table entry
    }
    
    return (found) ? 0 : 1;
}

u32 DebugSeekFileInNand(u32* offset, u32* size, const char* filename, const char* path, PartitionInfo* partition)
{
    Debug("Searching for %s...", filename);
    if (SeekFileInNand(offset, size, path, partition) != 0) {
        Debug("Failed!");
        return 1;
    }
    if (*size < 1024)
        Debug("Found at %08X, size %ub", *offset, *size);
    else if (*size < 1024 * 1024)
        Debug("Found at %08X, size %ukB", *offset, *size / 1024);
    else
        Debug("Found at %08X, size %uMB", *offset, *size / (1024*1024));
    
    return 0;
}

u32 DumpFile(u32 param)
{
    NandFileInfo* f_info = GetNandFileInfo(param);
    PartitionInfo* p_info = GetPartitionInfo(f_info->partition_id);
    u32 offset;
    u32 size;
    
    if (DebugSeekFileInNand(&offset, &size, (IsEmuNand()) ? f_info->name_emu : f_info->name_sys, f_info->path, p_info) != 0)
        return 1;
    if (DecryptNandToFile((IsEmuNand()) ? f_info->name_emu : f_info->name_sys, offset, size, p_info) != 0)
        return 1;
    
    return 0;
}

u32 InjectFile(u32 param)
{
    NandFileInfo* f_info = GetNandFileInfo(param);
    PartitionInfo* p_info = GetPartitionInfo(f_info->partition_id);
    u32 offset;
    u32 size;
    
    if (DebugSeekFileInNand(&offset, &size, f_info->name_sys, f_info->path, p_info) != 0)
        return 1;
    if (EncryptFileToNand(f_info->name_sys, offset, size, p_info) != 0)
        return 1;
    
    return 0;
}
