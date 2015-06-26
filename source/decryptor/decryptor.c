#include <string.h>

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

u8* ctrStart = NULL;

// From https://github.com/profi200/Project_CTR/blob/master/makerom/pki/prod.h#L19
static const u8 common_keyy[6][16] = {
    {0xD0, 0x7B, 0x33, 0x7F, 0x9C, 0xA4, 0x38, 0x59, 0x32, 0xA2, 0xE2, 0x57, 0x23, 0x23, 0x2E, 0xB9} , // 0 - eShop Titles
    {0x0C, 0x76, 0x72, 0x30, 0xF0, 0x99, 0x8F, 0x1C, 0x46, 0x82, 0x82, 0x02, 0xFA, 0xAC, 0xBE, 0x4C} , // 1 - System Titles
    {0xC4, 0x75, 0xCB, 0x3A, 0xB8, 0xC7, 0x88, 0xBB, 0x57, 0x5E, 0x12, 0xA1, 0x09, 0x07, 0xB8, 0xA4} , // 2
    {0xE4, 0x86, 0xEE, 0xE3, 0xD0, 0xC0, 0x9C, 0x90, 0x2F, 0x66, 0x86, 0xD4, 0xC0, 0x6F, 0x64, 0x9F} , // 3
    {0xED, 0x31, 0xBA, 0x9C, 0x04, 0xB0, 0x67, 0x50, 0x6C, 0x44, 0x97, 0xA3, 0x5B, 0x78, 0x04, 0xFC} , // 4
    {0x5E, 0x66, 0x99, 0x8A, 0xB4, 0xE8, 0x93, 0x16, 0x06, 0x85, 0x0F, 0xD7, 0xA1, 0x6D, 0xD7, 0x55} , // 5
};

u32 DecryptBuffer(DecryptBufferInfo *info)
{
    u8 ctr[16] __attribute__((aligned(32)));
    memcpy(ctr, info->CTR, 16);

    u8* buffer = info->buffer;
    u32 size = info->size;

    if (info->setKeyY) {
        setup_aeskey(info->keyslot, AES_BIG_INPUT | AES_NORMAL_INPUT, info->keyY);
        info->setKeyY = 0;
    }
    use_aeskey(info->keyslot);

    for (u32 i = 0; i < size; i += 0x10, buffer += 0x10) {
        set_ctr(AES_BIG_INPUT | AES_NORMAL_INPUT, ctr);
        aes_decrypt((void*) buffer, (void*) buffer, ctr, 1, AES_CTR_MODE);
        add_ctr(ctr, 0x1);
    }

    memcpy(info->CTR, ctr, 16);

    return 0;
}

u32 DecryptTitlekeys(void)
{
    EncKeysInfo *info = (EncKeysInfo*)0x20316000;

    Debug("Opening encTitleKeys.bin ...");
    if (!FileOpen("/encTitleKeys.bin")) {
        Debug("Could not open encTitleKeys.bin!");
        return 1;
    }
    FileRead(info, 16, 0);

    if (!info->n_entries || info->n_entries > MAX_ENTRIES) {
        Debug("Too many/few entries specified: %i", info->n_entries);
        FileClose();
        return 1;
    }

    Debug("Number of entries: %i", info->n_entries);

    FileRead(info->entries, info->n_entries * sizeof(TitleKeyEntry), 16);
    FileClose();

    Debug("Decrypting Title Keys...");

    u8 ctr[16] __attribute__((aligned(32)));
    u8 keyY[16] __attribute__((aligned(32)));
    u32 i;
    for (i = 0; i < info->n_entries; i++) {
        memset(ctr, 0, 16);
        memcpy(ctr, info->entries[i].titleId, 8);
        set_ctr(AES_BIG_INPUT|AES_NORMAL_INPUT, ctr);
        memcpy(keyY, (void *)common_keyy[info->entries[i].commonKeyIndex], 16);
        setup_aeskey(0x3D, AES_BIG_INPUT|AES_NORMAL_INPUT, keyY);
        use_aeskey(0x3D);
        aes_decrypt(info->entries[i].encryptedTitleKey, info->entries[i].encryptedTitleKey, ctr, 1, AES_CBC_DECRYPT_MODE);
    }

    if (!FileCreate("/decTitleKeys.bin", true))
        return 1;

    FileWrite(info, info->n_entries * sizeof(TitleKeyEntry) + 16, 0);
    FileClose();

    Debug("Done!");

    return 0;
}

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
        if (!seedinfo->n_entries || seedinfo->n_entries > MAX_ENTRIES) {
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

    if (!info->n_entries || info->n_entries > MAX_ENTRIES || (info->ncch_info_version != 0xF0000004)) {
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

    if (!info->n_entries || info->n_entries > MAX_ENTRIES) {
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

u32 SeekMagicNumber(u8* magic, u32 magiclen, u32 offset, u32 size, u32 keyslot)
{
	DecryptBufferInfo info;
    u8* buffer = BUFFER_ADDRESS;
	u32 found = (u32) -1;

	if (ctrStart == NULL) {
		ctrStart = FindNandCtr();
        if (ctrStart == NULL)
            return 1;
	}

    info.keyslot = keyslot;
    info.setKeyY = 0;
    info.size = NAND_SECTOR_SIZE;
    info.buffer = buffer;
    for (u32 i = 0; i < 16; i++) {
        info.CTR[i] = *(ctrStart + (0xF - i)); // The CTR is stored backwards in memory.
    }
    add_ctr(info.CTR, offset / 0x10);

    u32 n_sectors = size / NAND_SECTOR_SIZE;
    u32 start_sector = offset / NAND_SECTOR_SIZE;
    for (u32 i = 0; i < n_sectors; i++) {
        ShowProgress(i, n_sectors);
        sdmmc_nand_readsectors(start_sector + i, 1, buffer);
        DecryptBuffer(&info);
		if(memcmp(buffer, magic, magiclen) == 0) {
			found = (start_sector + i) * NAND_SECTOR_SIZE;
			break;
		}
    }

    ShowProgress(0, 0);
    FileClose();

    return found;
}

u32 DumpPartition(char* filename, u32 offset, u32 size, u32 keyslot)
{
    DecryptBufferInfo info;
    u8* buffer = BUFFER_ADDRESS;

    Debug("Decrypting NAND data. Size (MB): %u", offset, size);
    Debug("Filename: %s", filename);

	if (ctrStart == NULL) {
		ctrStart = FindNandCtr();
        if (ctrStart == NULL)
            return 1;
	}

    info.keyslot = keyslot;
    info.setKeyY = 0;
    info.size = SECTORS_PER_READ * NAND_SECTOR_SIZE;
    info.buffer = buffer;
    for (u32 i = 0; i < 16; i++) {
        info.CTR[i] = *(ctrStart + (0xF - i)); // The CTR is stored backwards in memory.
    }
    add_ctr(info.CTR, offset / 0x10);

    if (!FileCreate(filename, true))
        return 1;

    u32 n_sectors = size / NAND_SECTOR_SIZE;
    u32 start_sector = offset / NAND_SECTOR_SIZE;
    for (u32 i = 0; i < n_sectors; i += SECTORS_PER_READ) {
		u32 read_sectors = min(SECTORS_PER_READ, (n_sectors - i));
        ShowProgress(i, n_sectors);
        sdmmc_nand_readsectors(start_sector + i, read_sectors, buffer);
        DecryptBuffer(&info);
        FileWrite(buffer, NAND_SECTOR_SIZE * read_sectors, i * NAND_SECTOR_SIZE);
    }

    ShowProgress(0, 0);
    FileClose();

    return 0;
}

u32 NandPadgen()
{
    if (ctrStart == NULL) {
		ctrStart = FindNandCtr();
        if (ctrStart == NULL)
            return 1;
	}

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

u32 NandDumper()
{
    u8* buffer = BUFFER_ADDRESS;
    u32 nand_size = (GetUnitPlatform() == PLATFORM_3DS) ? 0x3AF00000 : 0x4D800000;

    Debug("Dumping System NAND. Size (MB): %u", nand_size / (1024 * 1024));
    Debug("Filename: NAND.bin");

    if (!FileCreate("/NAND.bin", true))
        return 1;

    u32 n_sectors = nand_size / NAND_SECTOR_SIZE;
    for (u32 i = 0; i < n_sectors; i += SECTORS_PER_READ) {
        ShowProgress(i, n_sectors);
        sdmmc_nand_readsectors(i, SECTORS_PER_READ, buffer);
        FileWrite(buffer, NAND_SECTOR_SIZE * SECTORS_PER_READ, i * NAND_SECTOR_SIZE);
    }

    ShowProgress(0, 0);
    FileClose();

    return 0;
}

u32 NandPartitionsDumper() {
    u32 ctrnand_offset;
    u32 ctrnand_size;
    u32 keyslot;

    if(GetUnitPlatform() == PLATFORM_3DS) {
        ctrnand_offset = 0x0B95CA00;
        ctrnand_size = 0x2F3E3600;
        keyslot = 0x4;
	} else {
        ctrnand_offset = 0x0B95AE00;
        ctrnand_size = 0x41D2D200;
        keyslot = 0x5;
    }

    // see: http://3dbrew.org/wiki/Flash_Filesystem
    Debug("Dumping firm0.bin: %s!", DumpPartition("firm0.bin", 0x0B130000, 0x00400000, 0x6) == 0 ? "succeeded" : "failed");
    Debug("Dumping firm1.bin: %s!", DumpPartition("firm1.bin", 0x0B530000, 0x00400000, 0x6) == 0 ? "succeeded" : "failed");
    Debug("Dumping ctrnand.bin: %s!", DumpPartition("ctrnand.bin", ctrnand_offset, ctrnand_size, keyslot) == 0 ? "succeeded" : "failed");

    return 0;
}

u32 TicketDumper() {
	const u32 ticket_size = 0xD0000;
	u32 ctrnand_offset;
    u32 ctrnand_size;
    u32 keyslot;
	u32 offset;

    if(GetUnitPlatform() == PLATFORM_3DS) {
        ctrnand_offset = 0x0B95CA00;
        ctrnand_size = 0x2F3E3600;
        keyslot = 0x4;
	} else {
        ctrnand_offset = 0x0B95AE00;
        ctrnand_size = 0x41D2D200;
        keyslot = 0x5;
    }
	
    Debug("Seeking for 'TICK'...");
	offset = SeekMagicNumber((u8*) "TICK", 4, ctrnand_offset, ctrnand_size - ticket_size + NAND_SECTOR_SIZE, keyslot);
	if(offset == (u32) -1) {
		Debug("Failed!");
		return 1;
	}
	Debug("Found at 0x%08X", offset);
	
	return DumpPartition("ticket.bin", offset, ticket_size, keyslot);
}
