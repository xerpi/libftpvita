/*
 * Copyright (c) 2015-2016 Sergi Granell (xerpi)
 */

#ifndef FTPVITA_H
#define FTPVITA_H

#include <psp2/types.h>

/* Returns PSVita's IP and FTP port. 0 on success */
int ftpvita_init(char *vita_ip, unsigned short int *vita_port);
void ftpvita_fini();
int ftpvita_is_initialized();
int ftpvita_add_device(const char *devname);
int ftpvita_del_device(const char *devname);


#endif
