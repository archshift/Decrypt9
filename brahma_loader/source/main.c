#include <3ds.h>
#include "brahma.h"


s32 main (void) {
	srvInit();
	aptInit();
	hidInit(NULL);
	gfxInitDefault();
	fsInit();
	sdmcInit();
	hbInit();
	qtmInit();
	
	gfxSwapBuffers();
	
	if (brahma_init()) {
		load_arm9_payload(PAYLOAD);
		firm_reboot();
		brahma_exit();
	}
	
	hbExit();
	sdmcExit();
	fsExit();
	gfxExit();
	hidExit();
	aptExit();
	srvExit();
	
	return 0;
}
