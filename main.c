/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#include <string.h>

#include <psp2/display.h>
#include <psp2/ctrl.h>
#include <psp2/apputil.h>

#include "utils.h"
#include "console.h"
#include "ftpvita.h"

int main()
{
	char vita_ip[16];
	unsigned short int vita_port;
	SceAppUtilInitParam init_param;
	SceAppUtilBootParam boot_param;

	init_video();
	console_init();

	console_set_color(PURP);
	INFO("FTPVita by xerpi\n");
	console_set_color(CYAN);
	INFO("Press [] to exit\n");
	console_set_color(WHITE);

	// Init SceAppUtil
	memset(&init_param, 0, sizeof(SceAppUtilInitParam));
	memset(&boot_param, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&init_param, &boot_param);

	ftpvita_init(vita_ip, &vita_port);

	// Mount music0: and photo0:
	if (sceAppUtilMusicMount() == 0)
		ftpvita_add_device("music0:");
	if (sceAppUtilPhotoMount() == 0)
		ftpvita_add_device("photo0:");

	console_set_color(LIME);
	INFO("PSVita listening on IP %s Port %i\n", vita_ip, vita_port);
	console_set_color(WHITE);

	console_set_top_margin(10 + 20*3);

	/* Input variables */
	SceCtrlData pad;

	while (1) {
		sceCtrlPeekBufferPositive(0, &pad, 1);

		if (pad.buttons & SCE_CTRL_SQUARE) break;

		sceDisplayWaitVblankStart();
	}

	INFO("Exiting...\n");
	ftpvita_fini();

	// Unmount
	sceAppUtilPhotoUmount();
	sceAppUtilMusicUmount();

	// Shutdown AppUtil
	sceAppUtilShutdown();

	console_fini();
	end_video();
	return 0;
}
