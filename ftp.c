/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static int client_threads_run;
static int number_clients = 0;

typedef struct {
	int num;
	SceUID thid;
	int sockfd;
	SceNetSockaddrIn addr;
} ClientInfo;

static int client_thread(SceSize args, void *argp)
{
	DEBUG("Client thread started!\n");

	ClientInfo *client = (ClientInfo *)argp;

	while (client_threads_run) {

		sceKernelDelayThread(100*1000);
	}


	sceNetSocketClose(client->sockfd);
	free(client);

	DEBUG("Client thread exiting!\n");
	return 0;
}


static int server_thread(SceSize args, void *argp)
{
	int ret;
	int server_sockfd;
	SceNetSockaddrIn serveraddr;
	SceNetCtlInfo info;

	DEBUG("Server thread started!\n");

	/* Create server socket */
	server_sockfd = sceNetSocket("FTPVita_server_sock",
		PSP2_NET_AF_INET,
		PSP2_NET_SOCK_STREAM,
		0);

	DEBUG("Server socket fd: %d\n", server_sockfd);

	/* Fill the server's address */
	serveraddr.sin_family = PSP2_NET_AF_INET;
	serveraddr.sin_addr.s_addr = sceNetHtonl(PSP2_NET_INADDR_ANY);
	serveraddr.sin_port = sceNetHtons(FTP_PORT);

	/* Bind the server's address to the socket */
	ret = sceNetBind(server_sockfd, (SceNetSockaddr *)&serveraddr, sizeof(serveraddr));
	DEBUG("sceNetBind(): 0x%08X\n", ret);

	/* Start listening */
	ret = sceNetListen(server_sockfd, 128);
	DEBUG("sceNetListen(): 0x%08X\n", ret);

	/* Get IP address */
	ret = sceNetCtlInetGetInfo(PSP2_NETCTL_INFO_GET_IP_ADDRESS, &info);
	DEBUG("sceNetCtlInetGetInfo(): 0x%08X\n", ret);
	INFO("PSVita listening on IP %s Port %i\n", info.ip_address, FTP_PORT);

	while (server_thread_run) {

		/* Accept clients */
		SceNetSockaddrIn clientaddr;
		int client_sockfd;
		unsigned int addrlen = sizeof(clientaddr);

		DEBUG("Waiting for incoming connections...\n");

		client_sockfd = sceNetAccept(server_sockfd, (SceNetSockaddr *)&clientaddr, &addrlen);
		if (client_sockfd > 0) {

			DEBUG("New connection, client fd: 0x%08X\n", client_sockfd);

			/* Get the client's IP address */
			char remote_ip[16];
			sceNetInetNtop(PSP2_NET_AF_INET,
				&clientaddr.sin_addr.s_addr,
				remote_ip,
				sizeof(remote_ip));

			DEBUG("\tRemote IP: %s Remote port: %i\n",
				remote_ip,
				clientaddr.sin_port);

			/* Create a new thread for the client */
			char client_thread_name[64];
			sprintf(client_thread_name, "FTPVita_client_%i_thread",
				number_clients);

			SceUID client_thid = sceKernelCreateThread(
				client_thread_name, client_thread,
				0x10000100, 0x100000, 0, 0, NULL);

			DEBUG("Client %i thread UID: 0x%08X\n", number_clients, client_thid);

			/* Allocate the ClientInfo struct for the new client */
			ClientInfo *clinfo = malloc(sizeof(*clinfo));
			clinfo->num = number_clients;
			clinfo->thid = client_thid;
			clinfo->sockfd = client_sockfd;
			memcpy(&clinfo->addr, &clientaddr, sizeof(clinfo->addr));

			/* Start the client thread */
			sceKernelStartThread(client_thid, 1, clinfo);

			number_clients++;
		}


		sceKernelDelayThread(100*1000);
	}

	sceNetSocketClose(server_sockfd);

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
	client_threads_run = 1;
	server_thread_run = 1;
	sceKernelStartThread(server_thid, 0, NULL);

	ftp_initialized = 1;
}

void ftp_fini()
{
	if (ftp_initialized) {
		server_thread_run = 0;
		client_threads_run = 0;
		sceKernelDeleteThread(server_thid);
		/* UGLY: Give 50 ms for the threads to exit */
		sceKernelDelayThread(50*1000);
		sceNetCtlTerm();
		sceNetTerm();
	}
}

