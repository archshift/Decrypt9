#pragma once

u32 SetNand(bool use_emunand);

u32 NcchPadgen(u32 param);
u32 SdPadgen(u32 param);
u32 CtrNandPadgen(u32 param);
u32 TwlNandPadgen(u32 param);
u32 DumpSeedsave(u32 param);
u32 UpdateSeedDb(u32 param);
u32 DumpTicket(u32 param);
u32 DecryptTitlekeysFile(u32 param);
u32 DecryptTitlekeysNand(u32 param);
u32 DumpNand(u32 param);
u32 DecryptAllNandPartitions(u32 param);
u32 DecryptTwlNandPartition(u32 param);
u32 DecryptCtrNandPartition(u32 param);
u32 DecryptNcsdNcchBatch(u32 param);

u32 RestoreNand(u32 param);
u32 InjectAllNandPartitions(u32 param);
u32 InjectTwlNandPartition(u32 param);
u32 InjectCtrNandPartition(u32 param);
