/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#ifndef UTILS_H
#define UTILS_H

#define align_mem(addr, align) (((addr) + ((align) - 1)) & ~((align) - 1))
#define lerp(value, from_max, to_max) ((((value*10) * (to_max*10))/(from_max*10))/10)
#define abs(x) ((x) < 0 ? (-x) : (x))

/* SceCtrl */

typedef struct {
	uint64_t 	timeStamp;
	unsigned int 	buttons;
	unsigned char 	lx;
	unsigned char 	ly;
	unsigned char 	rx;
	unsigned char 	ry;
	uint8_t 	reserved[16];
} CtrlData;


#endif
