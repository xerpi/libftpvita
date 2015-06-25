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
	char vita_ip[16];
	unsigned short int vita_port;

	init_video();
	console_init();

	console_set_color(PURP);
	INFO("FTPVita by xerpi\n");
	console_set_color(CYAN);
	INFO("Press START to exit\n");
	console_set_color(WHITE);

	ftp_init(vita_ip, &vita_port);

	console_set_color(LIME);
	INFO("PSVita listening on IP %s Port %i\n", vita_ip, vita_port);
	console_set_color(WHITE);

	console_set_top_margin(10 + 20*3);

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
