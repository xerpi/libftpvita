/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/time.h>

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/moduleinfo.h>
#include <psp2/kernel/processmgr.h>

#include "utils.h"
#include "console.h"
#include "net.h"
#include "ftp.h"


int _start()
{
	init_video();
	console_init();

	console_set_color(CYAN);
	INFO("FTPVita by xerpi\n");
	INFO("Press START to exit\n");
	console_set_color(WHITE);

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
