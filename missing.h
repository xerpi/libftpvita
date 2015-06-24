/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

/* This should be on the PSP2SDK!! */

#ifndef MISSING_H
#define MISSING_H

#include <psp2/types.h>

/* SceLibKernel */

typedef struct SceKernelMutexOptParam {
	unsigned int size;
	int ceilingPriority;
} SceKernelMutexOptParam;

typedef struct SceKernelMutexInfo {
	unsigned int size;
	int mutexId;
	char name[32];
	unsigned int attr;
	int initCount;
	int currentCount;
	int currentOwnerId;
	int numWaitThreads;
} SceKernelMutexInfo;

SceUID sceKernelCreateMutex(const char *pName, unsigned int attr, int initCount, const SceKernelMutexOptParam *pOptParam);
int sceKernelDeleteMutex(SceUID mutexId);
int sceKernelLockMutex(SceUID mutexId, int lockCount, unsigned int *pTimeout);
int sceKernelLockMutexCB(SceUID mutexId, int lockCount, unsigned int *pTimeout);
int sceKernelTryLockMutex(SceUID mutexId, int lockCount);
int sceKernelUnlockMutex(SceUID mutexId, int unlockCount);
int sceKernelCancelMutex(SceUID mutexId, int newCount, int *pNumWaitThreads);
int sceKernelGetMutexInfo(SceUID mutexId, SceKernelMutexInfo *pInfo);


int sceKernelDelayThread(unsigned int usec);

typedef struct SceIoStat {
	SceMode 	st_mode;
	unsigned int 	st_attr;
	SceOff 		st_size;
	SceDateTime 	st_ctime;
	SceDateTime 	st_atime;
	SceDateTime 	st_mtime;
	unsigned int 	st_private[6];
} SceIoStat;

typedef struct SceIoDirent {
	SceIoStat 	d_stat;
	char 		d_name[256];
	void * 		d_private;
	int 		dummy;
} SceIoDirent;

enum IOAccessModes {
	FIO_S_IFMT		= 0xF000,
	FIO_S_IFLNK		= 0x4000,
	FIO_S_IFDIR		= 0x1000,
	FIO_S_IFREG		= 0x2000,
	FIO_S_ISUID		= 0x0800,
	FIO_S_ISGID		= 0x0400,
	FIO_S_ISVTX		= 0x0200,
	FIO_S_IRWXU		= 0x01C0,
	FIO_S_IRUSR		= 0x0100,
	FIO_S_IWUSR		= 0x0080,
	FIO_S_IXUSR		= 0x0040,
	FIO_S_IRWXG		= 0x0038,
	FIO_S_IRGRP		= 0x0020,
	FIO_S_IWGRP		= 0x0010,
	FIO_S_IXGRP		= 0x0008,
	FIO_S_IRWXO		= 0x0007,
	FIO_S_IROTH		= 0x0004,
	FIO_S_IWOTH		= 0x0002,
	FIO_S_IXOTH		= 0x0001,
};

// File mode checking macros
#define FIO_S_ISLNK(m)	(((m) & FIO_S_IFMT) == FIO_S_IFLNK)
#define FIO_S_ISREG(m)	(((m) & FIO_S_IFMT) == FIO_S_IFREG)
#define FIO_S_ISDIR(m)	(((m) & FIO_S_IFMT) == FIO_S_IFDIR)

enum IOFileModes {
	FIO_SO_IFMT		= 0x0038,		// Format mask
	FIO_SO_IFLNK		= 0x0008,		// Symbolic link
	FIO_SO_IFDIR		= 0x0010,		// Directory
	FIO_SO_IFREG		= 0x0020,		// Regular file
	FIO_SO_IROTH		= 0x0004,		// read
	FIO_SO_IWOTH		= 0x0002,		// write
	FIO_SO_IXOTH		= 0x0001,		// execute
};

// File mode checking macros
#define FIO_SO_ISLNK(m)	(((m) & FIO_SO_IFMT) == FIO_SO_IFLNK)
#define FIO_SO_ISREG(m)	(((m) & FIO_SO_IFMT) == FIO_SO_IFREG)
#define FIO_SO_ISDIR(m)	(((m) & FIO_SO_IFMT) == FIO_SO_IFDIR)


SceUID sceIoDopen(const char *dirname);
int sceIoDread(SceUID fd, SceIoDirent *dir);
int sceIoDclose(SceUID fd);

#endif
