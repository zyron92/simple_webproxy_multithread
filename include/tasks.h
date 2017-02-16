#ifndef __TASKS_H__
#define __TASKS_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include "socket_helper.h"
#include "http_handler.h"

/*
 * Data structure of socket pool
 */
typedef struct _pool{
	fd_set active_read_set;
	fd_set ready_read_set;
	int clientfd[FD_SETSIZE];
	struct sockaddr_in clientAddress[FD_SETSIZE];
	int max_active_read_fd;

	int maxi;
	int num_ready;
}Pool;

/*
 * Data structure of arguments of thread
 */
typedef struct _sthread{
	int client_fd;
	struct sockaddr_in clientAddress;
	int *client_fd_ptradr;
}sthread;

/*
 * Mutexes
 */
pthread_mutex_t mutex_log; //for accessing log file
pthread_mutex_t mutex_clientfd; //for the client socket file description

/*
 * Storing arguments for thread
 */
sthread thread_args[FD_SETSIZE];

/*
 * Initialisation of socket pool
 */
void init_pool(int sock_fd, Pool *p);

/*
 * Add a new client to the Pool and create a worker thread
 */
void add_client(int connfd, struct sockaddr_in clientAddress, Pool *p);

/* 
 * Check added clients and create worker thread for them
 */
void check_clients(Pool *p);

/*
 * The main task for the worker thread - handling each client request
 */
void *main_task(void *thread_args);

/*
 * Logging for Proxy Server
 */
int logging(char *clientIpAdd, int clientPortNum, HttpPacket *packet);

#endif
