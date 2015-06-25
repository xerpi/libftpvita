/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#include <psp2/display.h>
#include <psp2/ctrl.h>

#include "utils.h"
#include "console.h"
#include "ftp.h"


int _start()
{
	init_video();
	console_init();

	console_set_color(PURP);
	INFO("FTPVita by xerpi\n");
	console_set_color(CYAN);
	INFO("Press START to exit\n");
	console_set_color(WHITE);

	console_set_top_margin(console_get_y() + 20);

	ftp_init();

	/* Input variables */
	CtrlData pad;

	while (1) {
		sceCtrlPeekBufferPositive(0, (SceCtrlData *)&pad, 1);

		if (pad.buttons & PSP2_CTRL_START) break;

		sceDisplayWaitVblankStart();
	}

	ftp_fini();
	console_fini();
	end_video();
	return 0;
}
