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
	int ctrl_sockfd;
	int data_sockfd;
	SceNetSockaddrIn addr;
	int n_recv;
	char recv_buffer[512];
} ClientInfo;

typedef void (*cmd_dispatch_func)(ClientInfo *client);

typedef struct {
	const char *cmd;
	cmd_dispatch_func func;
} cmd_dispatch_entry;

#define client_sendstr_ctrl(cl, str) \
	sceNetSend(cl->ctrl_sockfd, str, strlen(str), 0)


static void cmd_USER_func(ClientInfo *client)
{
	client_sendstr_ctrl(client, "331 Username OK, need password b0ss.\n");
}

static void cmd_PASS_func(ClientInfo *client)
{
	client_sendstr_ctrl(client, "230 User logged in!\n");
}

static void cmd_QUIT_func(ClientInfo *client)
{
	client_sendstr_ctrl(client, "221 Goodbye senpai :'(\n");
}

static void cmd_SYST_func(ClientInfo *client)
{
	client_sendstr_ctrl(client, "215 UNIX Type: L8\n");
}

void cmd_PORT_func(ClientInfo *client)
{
	int ret;
	unsigned char active_ip[4];
	unsigned char porthi, portlo;
	unsigned short active_port;
	char ip_str[16];
	SceNetInAddr active_addr;
	SceNetSockaddrIn active_sockaddr;

	sscanf(client->recv_buffer, "%*s %hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
		&active_ip[0], &active_ip[1], &active_ip[2], &active_ip[3],
		&porthi, &portlo);

	active_port = portlo + porthi*256;

	/* Convert to an X.X.X.X IP string */
	sprintf(ip_str, "%d.%d.%d.%d",
		active_ip[0], active_ip[1], active_ip[2], active_ip[3]);

	/* Convert the IP to a SceNetInAddr */
	sceNetInetPton(PSP2_NET_AF_INET, ip_str, &active_addr);

	DEBUG("PORT connection to client's IP: %s Port: %d\n", ip_str, active_port);

	/* Create active mode socket name */
	char active_socket_name[64];
	sprintf(active_socket_name, "FTPVita_client_%i_active_socket",
		client->num);

	/* Create active mode socket */
	client->data_sockfd = sceNetSocket(active_socket_name,
		PSP2_NET_AF_INET,
		PSP2_NET_SOCK_STREAM,
		0);

	DEBUG("Client %i active socket fd: %d\n", client->num,
		client->data_sockfd);

	/* Fill active mode socket address */
	active_sockaddr.sin_family = PSP2_NET_AF_INET;
	active_sockaddr.sin_addr = active_addr;
	active_sockaddr.sin_port = sceNetHtons(active_port);

	/* Connect to the client using the new socket */
	ret = sceNetConnect(client->data_sockfd, (SceNetSockaddr *)&active_sockaddr,
		sizeof(active_sockaddr));
	DEBUG("sceNetConnect(): 0x%08X\n", ret);

	client_sendstr_ctrl(client, "200 PORT command successful!\n");
}

#define add_entry(name) {#name, cmd_##name##_func}
static cmd_dispatch_entry cmd_dispatch_table[] = {
	add_entry(USER),
	add_entry(PASS),
	add_entry(QUIT),
	add_entry(SYST),
	add_entry(PORT),
	{NULL, NULL}
};

static cmd_dispatch_func get_dispatch_func(const char *cmd)
{
	int i;
	for(i = 0; cmd_dispatch_table[i].cmd && cmd_dispatch_table[i].func; i++) {
		if (strcmp(cmd, cmd_dispatch_table[i].cmd) == 0) {
			return cmd_dispatch_table[i].func;
		}
	}
	return NULL;
}

static int client_thread(SceSize args, void *argp)
{
	char cmd[16];
	cmd_dispatch_func dispatch_func;
	ClientInfo *client = (ClientInfo *)argp;

	DEBUG("Client thread %i started!\n", client->num);

	client_sendstr_ctrl(client, "220 FTPVita Server ready.\n");

	while (client_threads_run) {

		memset(client->recv_buffer, 0, sizeof(client->recv_buffer));

		client->n_recv = sceNetRecv(client->ctrl_sockfd, client->recv_buffer, sizeof(client->recv_buffer), 0);
		if (client->n_recv > 0) {
			INFO("Received %i bytes from client number %i:\n\t> %s",
				client->n_recv, client->num, client->recv_buffer);

			/* The command are the first chars until the first space */
			sscanf(client->recv_buffer, "%s", cmd);

			/* Wait 1 ms before sending any data */
			sceKernelDelayThread(1*1000);

			if ((dispatch_func = get_dispatch_func(cmd))) {
				dispatch_func(client);
			} else {
				client_sendstr_ctrl(client, "502 Sorry, command not implemented. :(\n");
			}


		} else if (client->n_recv == 0) {
			/* Value 0 means connection closed */
			INFO("Connection closed by the client %i\n", client->num);
			break;
		}

		sceKernelDelayThread(10*1000);
	}

	sceNetSocketClose(client->ctrl_sockfd);
	sceNetSocketClose(client->data_sockfd);

	DEBUG("Client thread %i exiting!\n", client->num);

	free(client);

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
				remote_ip, clientaddr.sin_port);

			/* Create a new thread for the client */
			char client_thread_name[64];
			sprintf(client_thread_name, "FTPVita_client_%i_thread",
				number_clients);

			SceUID client_thid = sceKernelCreateThread(
				client_thread_name, client_thread,
				0x10000100, 0x10000, 0, 0, NULL);

			DEBUG("Client %i thread UID: 0x%08X\n", number_clients, client_thid);

			/* Allocate the ClientInfo struct for the new client */
			ClientInfo *clinfo = malloc(sizeof(*clinfo));
			clinfo->num = number_clients;
			clinfo->thid = client_thid;
			clinfo->ctrl_sockfd = client_sockfd;
			memcpy(&clinfo->addr, &clientaddr, sizeof(clinfo->addr));

			/* Start the client thread */
			sceKernelStartThread(client_thid, sizeof(*clinfo), clinfo);

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
		server_thread, 0x10000100, 0x10000, 0, 0, NULL);
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

