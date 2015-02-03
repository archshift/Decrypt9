#include "titlekey.h"
#include "fs.h"
#include "draw.h"
#include "crypto.h"

u32 DecryptTitlekeys(void)
{
	struct enckeys_info *info = (struct enckeys_info *)((void *)0x20316000);
	
	Debug("Opening encTitleKeys.bin ...");
	if(!FileOpen("/encTitleKeys.bin", false))
	{
		Debug("Could not open encTitleKeys.bin!");
		return 1;
	}
	FileRead(info, 16, 0);
	
	if (!info->n_entries || info->n_entries > MAXENTRIES) {
		Debug("Too many/few entries specified: %i", info->n_entries);
		FileClose();
		return 1;
	}
	
	Debug("Number of entries: %i", info->n_entries);
	
	FileRead(info->entries, info->n_entries*sizeof(struct title_key_entry), 16);
	FileClose();
	
	Debug("Decrypting Title Keys...");
	
	u8 ctr[16] __attribute__((aligned(32)));
	u8 keyY[16] __attribute__((aligned(32)));
	u32 i;
	for(i = 0; i < info->n_entries; i++) {
		memset(ctr, 0, 16);
		memcpy(ctr, info->entries[i].titleId, 8);
		set_ctr(AES_BIG_INPUT|AES_NORMAL_INPUT, ctr);
		memcpy(keyY, (void *) (0x08097A8C + ((20 * info->entries[i].commonKeyIndex) + 5)), 16);
		setup_aeskey(0x3D, AES_BIG_INPUT|AES_NORMAL_INPUT, keyY);
		use_aeskey(0x3D);
		aes_decrypt(info->entries[i].encryptedTitleKey, info->entries[i].encryptedTitleKey, ctr, 1, AES_CBC_DECRYPT_MODE);
	}

	if(!FileOpen("/decTitleKeys.bin", true))
		return 1;

    FileWrite(info, info->n_entries*sizeof(struct title_key_entry)+16, 0);
	FileClose();
	
	Debug("Done!");
	
	return 0;
}
