#include <string.h>
#include <stdio.h>

#include "fs.h"
#include "draw.h"
#include "decryptor/features.h"
#include "decryptor/decryptor.h"
#include "decryptor/nand.h"
#include "decryptor/nandfat.h"


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

u32 DumpTicket(u32 param)
{
    PartitionInfo* ctrnand_info = GetPartitionInfo(P_CTRNAND);
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

u32 DumpSeedsave(u32 param)
{
    PartitionInfo* ctrnand_info = GetPartitionInfo(P_CTRNAND);
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
