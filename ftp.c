/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#include <stdlib.h>

#include <psp2/kernel/threadmgr.h>

#include "net.h"
#include "netctl.h"
#include "console.h"
#include "missing.h"

#define FTP_PORT 1337
#define NET_INIT_SIZE 0x4000
static int ftp_initialized = 0;
static SceUID server_thid;
static int server_thread_run;

static int server_thread(SceSize args, void *argp)
{
	int ret;
	int server_fd;
	SceNetSockaddrIn serveraddr;
	SceNetCtlInfo info;

	DEBUG("Server thread started!\n");

	/* Create server socket */
	server_fd = sceNetSocket("FTPVita_server_sock",
		PSP2_NET_AF_INET,
		PSP2_NET_SOCK_STREAM,
		0);

	DEBUG("Server socket fd: %d\n", server_fd);

	/* Fill the server's address */
	serveraddr.sin_family = PSP2_NET_AF_INET;
	serveraddr.sin_addr.s_addr = sceNetHtonl(PSP2_NET_INADDR_ANY);
	serveraddr.sin_port = sceNetHtons(FTP_PORT);

	/* Bind the server's address to the socket */
	ret = sceNetBind(server_fd, (SceNetSockaddr *)&serveraddr, sizeof(serveraddr));
	DEBUG("sceNetBind(): 0x%08X\n", ret);

	/* Start listening */
	ret = sceNetListen(server_fd, 128);
	DEBUG("sceNetListen(): 0x%08X\n", ret);

	/* Get IP address */
	ret = sceNetCtlInetGetInfo(PSP2_NETCTL_INFO_GET_IP_ADDRESS, &info);
	DEBUG("sceNetCtlInetGetInfo(): 0x%08X\n", ret);
	INFO("PSVita listening on IP %s Port %i\n", info.ip_address, FTP_PORT);

	while (server_thread_run) {

		/* Accept clients */
		SceNetSockaddrIn clientaddr;
		int client_fd;
		unsigned int addrlen = sizeof(clientaddr);

		DEBUG("Waiting for incoming connections...\n");

		client_fd = sceNetAccept(server_fd, (SceNetSockaddr *)&clientaddr, &addrlen);
		DEBUG("New connection, client fd: 0x%08X\n", client_fd);
		if (client_fd > 0) {

		}


		sceKernelDelayThread(100*1000);
	}

	sceNetSocketClose(server_fd);

	DEBUG("Server thread exiting!\n");
	return 0;
}


void ftp_init()
{
	int ret;
	SceNetInitParam initparam;

	if (ftp_initialized) {
		return;
	}

	/* Init Net */
	if (sceNetShowNetstat() == PSP2_NET_ERROR_ENOTINIT) {

		initparam.memory = malloc(NET_INIT_SIZE);
		initparam.size = NET_INIT_SIZE;
		initparam.flags = 0;

		ret = sceNetInit(&initparam);
		DEBUG("sceNetInit(): 0x%08X\n", ret);
	} else {
		DEBUG("Net is already initialized.\n");
	}

	/* Init NetCtl */
	ret = sceNetCtlInit();
	DEBUG("sceNetCtlInit(): 0x%08X\n", ret);

	/* Create server thread */
	server_thid = sceKernelCreateThread("FTPVita_server_thread",
		server_thread, 0x10000100, 0x100000, 0, 0, NULL);
	DEBUG("Server thread UID: 0x%08X\n", server_thid);

	/* Start the server thread */
	server_thread_run = 1;
	sceKernelStartThread(server_thid, 0, NULL);

	ftp_initialized = 1;
}

void ftp_fini()
{
	if (ftp_initialized) {
		server_thread_run = 0;
		sceKernelDeleteThread(server_thid);
		sceNetCtlTerm();
		sceNetTerm();
	}
}

