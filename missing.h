/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

/* This should be on the PSP2SDK!! */

#ifndef MISSING_H
#define MISSING_H

#include <psp2/types.h>

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

#endif
