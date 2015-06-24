/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslimits.h>

#include <psp2/kernel/threadmgr.h>

#include "net.h"
#include "netctl.h"
#include "console.h"
#include "missing.h"

#define FTP_PORT 1337
#define NET_INIT_SIZE 0x4000
#define FILE_BUF_SIZE 4*1024*1024

static int ftp_initialized = 0;
static SceUID server_thid;
static int server_thread_run;
static int client_threads_run;
static int number_clients = 0;

typedef enum {
	FTP_DATA_CONNECTION_NONE,
	FTP_DATA_CONNECTION_ACTIVE,
	FTP_DATA_CONNECTION_PASSIVE,
} DataConnectionType;

typedef struct {
	/* Client number */
	int num;
	/* Thread UID */
	SceUID thid;
	/* Control connection socket FD */
	int ctrl_sockfd;
	/* Data connection attributes */
	int data_sockfd;
	DataConnectionType data_con_type;
	SceNetSockaddrIn data_sockaddr;
	/* Remote client net info */
	SceNetSockaddrIn addr;
	/* Receive buffer attributes */
	int n_recv;
	char recv_buffer[512];
	/* Current working directory */
	char cur_path[PATH_MAX];
} ClientInfo;

typedef void (*cmd_dispatch_func)(ClientInfo *client);

typedef struct {
	const char *cmd;
	cmd_dispatch_func func;
} cmd_dispatch_entry;

#define client_send_ctrl_msg(cl, str) \
	sceNetSend(cl->ctrl_sockfd, str, strlen(str), 0)

#define client_send_data_msg(cl, str) \
	sceNetSend(cl->data_sockfd, str, strlen(str), 0)

static void cmd_USER_func(ClientInfo *client)
{
	client_send_ctrl_msg(client, "331 Username OK, need password b0ss.\n");
}

static void cmd_PASS_func(ClientInfo *client)
{
	client_send_ctrl_msg(client, "230 User logged in!\n");
}

static void cmd_QUIT_func(ClientInfo *client)
{
	client_send_ctrl_msg(client, "221 Goodbye senpai :'(\n");
}

static void cmd_SYST_func(ClientInfo *client)
{
	client_send_ctrl_msg(client, "215 UNIX Type: L8\n");
}

static void cmd_PORT_func(ClientInfo *client)
{
	unsigned char data_ip[4];
	unsigned char porthi, portlo;
	unsigned short data_port;
	char ip_str[16];
	SceNetInAddr data_addr;

	sscanf(client->recv_buffer, "%*s %hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
		&data_ip[0], &data_ip[1], &data_ip[2], &data_ip[3],
		&porthi, &portlo);

	data_port = portlo + porthi*256;

	/* Convert to an X.X.X.X IP string */
	sprintf(ip_str, "%d.%d.%d.%d",
		data_ip[0], data_ip[1], data_ip[2], data_ip[3]);

	/* Convert the IP to a SceNetInAddr */
	sceNetInetPton(PSP2_NET_AF_INET, ip_str, &data_addr);

	DEBUG("PORT connection to client's IP: %s Port: %d\n", ip_str, data_port);

	/* Create data mode socket name */
	char data_socket_name[64];
	sprintf(data_socket_name, "FTPVita_client_%i_data_socket",
		client->num);

	/* Create data mode socket */
	client->data_sockfd = sceNetSocket(data_socket_name,
		PSP2_NET_AF_INET,
		PSP2_NET_SOCK_STREAM,
		0);

	DEBUG("Client %i data socket fd: %d\n", client->num,
		client->data_sockfd);

	/* Prepare socket address for the data connection */
	client->data_sockaddr.sin_family = PSP2_NET_AF_INET;
	client->data_sockaddr.sin_addr = data_addr;
	client->data_sockaddr.sin_port = sceNetHtons(data_port);

	/* Set the data connection type to active! */
	client->data_con_type = FTP_DATA_CONNECTION_ACTIVE;

	client_send_ctrl_msg(client, "200 PORT command successful!\n");
}

static void client_open_data_connection(ClientInfo *client)
{
	int ret;

	if (client->data_con_type == FTP_DATA_CONNECTION_ACTIVE) {
		/* Connect to the client using the data socket */
		ret = sceNetConnect(client->data_sockfd,
			(SceNetSockaddr *)&client->data_sockaddr,
			sizeof(client->data_sockaddr));

		DEBUG("sceNetConnect(): 0x%08X\n", ret);
	} else {
		/* Listen from the client using the data socket */

	}
}

static void client_close_data_connection(ClientInfo *client)
{
	sceNetSocketClose(client->data_sockfd);
	client->data_con_type = FTP_DATA_CONNECTION_NONE;
}

static const char *num_to_month[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int gen_list_format(char *out, int n, int dir, unsigned int file_size,
	int month_n, int day_n, int hour, int minute, const char *filename)
{
	return snprintf(out, n,
		"%c%s 1 vita vita %d %s %-2d %02d:%02d %s\n",
		dir ? 'd' : '-',
		dir ? "rwxr-xr-x" : "rw-r--r--",
		file_size,
		num_to_month[(month_n-1)%12],
		day_n,
		hour,
		minute,
		filename);
}


static void send_LIST(ClientInfo *client, const char *path)
{
	char buffer[512];
	SceUID dir;
	SceIoDirent dirent;

	client_send_ctrl_msg(client, "150 Opening ASCII mode data transfer for LIST\n");

	client_open_data_connection(client);

	dir = sceIoDopen(path);
	memset(&dirent, 0, sizeof(dirent));

	while (sceIoDread(dir, &dirent) > 0) {
		gen_list_format(buffer, sizeof(buffer),
			FIO_S_ISDIR(dirent.d_stat.st_mode),
			dirent.d_stat.st_size,
			dirent.d_stat.st_ctime.month,
			dirent.d_stat.st_ctime.day,
			dirent.d_stat.st_ctime.hour,
			dirent.d_stat.st_ctime.minute,
			dirent.d_name);

		client_send_data_msg(client, buffer);
		memset(&dirent, 0, sizeof(dirent));
		memset(buffer, 0, sizeof(buffer));
	}

	sceIoDclose(dir);

	DEBUG("Done sending LIST\n");

	client_close_data_connection(client);
	client_send_ctrl_msg(client, "226 Transfer complete.\n");
}

static void cmd_LIST_func(ClientInfo *client)
{
	char list_path[PATH_MAX];

	int n = sscanf(client->recv_buffer, "%*s %[^\r\n\t]", list_path);

	if (n > 0) {  /* Client specified a path */
		send_LIST(client, list_path);
	} else {      /* Use current path */
		send_LIST(client, client->cur_path);
	}
}

static void cmd_PWD_func(ClientInfo *client)
{
	char msg[PATH_MAX];
	/* We don't want to send "pss0:" */
	const char *pwd_path = strchr(client->cur_path, '/');

	sprintf(msg, "257 \"%s\" is the current directory.\n", pwd_path);
	client_send_ctrl_msg(client, msg);
}

static void cmd_CWD_func(ClientInfo *client)
{
	char path[PATH_MAX];
	int n = sscanf(client->recv_buffer, "%*s %[^\r\n\t]", path);

	if (n < 1) {
		client_send_ctrl_msg(client, "500 Syntax error, command unrecognized.\n");
	} else {
		if (path[0] != '/') { /* Change dir relative to current dir */
			strcat(client->cur_path, path);
		} else {
			sprintf(client->cur_path, "pss0:%s", path);
		}

		/* If there isn't "/" at the end, add it */
		if (client->cur_path[strlen(client->cur_path) - 1] != '/') {
			strcat(client->cur_path, "/");
		}

		client_send_ctrl_msg(client, "250 Requested file action okay, completed.\n");
	}
}

static void cmd_TYPE_func(ClientInfo *client)
{
	char data_type;
	char format_control[8];
	int n_args = sscanf(client->recv_buffer, "%*s %c %s", &data_type, format_control);

	if (n_args > 0) {
		switch(data_type) {
		case 'A':
		case 'I':
			client_send_ctrl_msg(client, "200 Okay\n");
			break;
		case 'E':
		case 'L':
		default:
			client_send_ctrl_msg(client, "504 Error: bad parameters?\n");
			break;
		}
	} else {
		client_send_ctrl_msg(client, "504 Error: bad parameters?\n");
	}
}

static void dir_up(const char *in, char *out)
{
	const char *pch = strrchr(in, '/');
	if (pch && pch != in) {
		size_t s = pch - in;
		strncpy(out, in, s);
		out[s] = '\0';
	} else {
		strcpy(out, "/");
	}
}

static void cmd_CDUP_func(ClientInfo *client)
{
	int s_len = strlen(client->cur_path) + 1;
	char buf[s_len];
	memcpy(buf, client->cur_path, s_len);
	dir_up(buf, client->cur_path);
	client_send_ctrl_msg(client, "200 Command okay.\n");
}

static void send_file(ClientInfo *client, const char *path)
{
	unsigned char *buffer;
	SceUID fd;
	unsigned int bytes_read;

	DEBUG("Opening: %s\n", path);

	if ((fd = sceIoOpen(path, PSP2_O_RDONLY, 0777)) >= 0) {

		buffer = malloc(FILE_BUF_SIZE);
		if (buffer == NULL) {
			client_send_ctrl_msg(client, "550 Could not allocate memory.\n");
			return;
		}

		client_open_data_connection(client);
		client_send_ctrl_msg(client, "150 Opening Image mode data transfer.\n");

		while ((bytes_read = sceIoRead (fd, buffer, FILE_BUF_SIZE)) > 0) {
			sceNetSend(client->data_sockfd, buffer, bytes_read, 0);
		}

		sceIoClose(fd);
		free(buffer);
		client_send_ctrl_msg(client, "226 Transfer completed.\n");
		client_close_data_connection(client);

	} else {
		client_send_ctrl_msg(client, "550 File not found.\n");
	}
}

/* This functions generated a PSVita valid path with the input
 * from RETR, STOR, DELE, RMD and MKD commands */
static void gen_filepath(ClientInfo *client, char *dest_path)
{
	char cmd_path[PATH_MAX];
	sscanf(client->recv_buffer, "%*[^ ] %[^\r\n\t]", cmd_path);

	if (cmd_path[0] != '/') { /* The file is relative to current dir */
		/* Append the file to the current path */
		sprintf(dest_path, "%s%s", client->cur_path, cmd_path);
	} else {
		/* Add "pss0:" to the file */
		sprintf(dest_path, "pss0:%s", cmd_path);
	}
}

static void cmd_RETR_func(ClientInfo *client)
{
	char dest_path[PATH_MAX];
	gen_filepath(client, dest_path);
	send_file(client, dest_path);
}

static void receive_file(ClientInfo *client, const char *path)
{
	unsigned char *buffer;
	SceUID fd;
	unsigned int bytes_recv;

	DEBUG("Opening: %s\n", path);

	if ((fd = sceIoOpen(path, PSP2_O_CREAT | PSP2_O_WRONLY | PSP2_O_TRUNC, 0777)) >= 0) {

		buffer = malloc(FILE_BUF_SIZE);
		if (buffer == NULL) {
			client_send_ctrl_msg(client, "550 Could not allocate memory.\n");
			return;
		}

		client_open_data_connection(client);
		client_send_ctrl_msg(client, "150 Opening Image mode data transfer.\n");

		while ((bytes_recv = sceNetRecv(client->data_sockfd, buffer, FILE_BUF_SIZE, 0)) > 0) {
			sceIoWrite(fd, buffer, bytes_recv);
		}

		sceIoClose(fd);
		free(buffer);
		client_send_ctrl_msg(client, "226 Transfer completed.\n");
		client_close_data_connection(client);

	} else {
		client_send_ctrl_msg(client, "550 File not found.\n");
	}
}

static void cmd_STOR_func(ClientInfo *client)
{
	char dest_path[PATH_MAX];
	gen_filepath(client, dest_path);
	receive_file(client, dest_path);
}

static void delete_file(ClientInfo *client, const char *path)
{
	DEBUG("Deleting: %s\n", path);

	if (sceIoRemove(path) >= 0) {
		client_send_ctrl_msg(client, "226 File deleted.\n");
	} else {
		client_send_ctrl_msg(client, "550 Could not delete the file.\n");
	}
}

static void cmd_DELE_func(ClientInfo *client)
{
	char dest_path[PATH_MAX];
	gen_filepath(client, dest_path);
	delete_file(client, dest_path);
}

static void delete_dir(ClientInfo *client, const char *path)
{
	DEBUG("Deleting: %s\n", path);

	if (sceIoRmdir(path) >= 0) {
		client_send_ctrl_msg(client, "226 Directory deleted.\n");
	} else {
		client_send_ctrl_msg(client, "550 Could not delete the directory.\n");
	}
}

static void cmd_RMD_func(ClientInfo *client)
{
	char dest_path[PATH_MAX];
	gen_filepath(client, dest_path);
	delete_dir(client, dest_path);
}

static void create_dir(ClientInfo *client, const char *path)
{
	DEBUG("Creating: %s\n", path);

	if (sceIoMkdir(path, 0777) >= 0) {
		client_send_ctrl_msg(client, "226 Directory created.\n");
	} else {
		client_send_ctrl_msg(client, "550 Could not create the directory.\n");
	}
}

static void cmd_MKD_func(ClientInfo *client)
{
	char dest_path[PATH_MAX];
	gen_filepath(client, dest_path);
	create_dir(client, dest_path);
}


#define add_entry(name) {#name, cmd_##name##_func}
static cmd_dispatch_entry cmd_dispatch_table[] = {
	add_entry(USER),
	add_entry(PASS),
	add_entry(QUIT),
	add_entry(SYST),
	add_entry(PORT),
	add_entry(LIST),
	add_entry(PWD),
	add_entry(CWD),
	add_entry(TYPE),
	add_entry(CDUP),
	add_entry(RETR),
	add_entry(STOR),
	add_entry(DELE),
	add_entry(RMD),
	add_entry(MKD),
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

	client_send_ctrl_msg(client, "220 FTPVita Server ready.\n");

	while (client_threads_run) {

		memset(client->recv_buffer, 0, sizeof(client->recv_buffer));

		client->n_recv = sceNetRecv(client->ctrl_sockfd, client->recv_buffer, sizeof(client->recv_buffer), 0);
		if (client->n_recv > 0) {
			DEBUG("Received %i bytes from client number %i:\n",
				client->n_recv, client->num);

			INFO("\t> %s", client->recv_buffer);

			/* The command are the first chars until the first space */
			sscanf(client->recv_buffer, "%s", cmd);

			/* Wait 1 ms before sending any data */
			sceKernelDelayThread(1*1000);

			if ((dispatch_func = get_dispatch_func(cmd))) {
				dispatch_func(client);
			} else {
				client_send_ctrl_msg(client, "502 Sorry, command not implemented. :(\n");
			}


		} else if (client->n_recv == 0) {
			/* Value 0 means connection closed */
			INFO("Connection closed by the client %i.\n", client->num);
			break;
		}

		sceKernelDelayThread(10*1000);
	}

	sceNetSocketClose(client->ctrl_sockfd);

	/* If there's an open data connection, close it */
	if (client->data_con_type != FTP_DATA_CONNECTION_NONE) {
		sceNetSocketClose(client->data_sockfd);
	}

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

	console_set_color(LIME);
	INFO("PSVita listening on IP %s Port %i\n", info.ip_address, FTP_PORT);
	console_set_color(WHITE);

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
			clinfo->data_con_type = FTP_DATA_CONNECTION_NONE;
			strcpy(clinfo->cur_path, "pss0:/top/Documents/");
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

