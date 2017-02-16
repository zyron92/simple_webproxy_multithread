#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "socket_helper.h"
#include "tasks.h"

int main()
{
	//STEP 0 : Preparing the proxy server, the connection pool 
	//         and client information
	int server_fd = server_listening();
	if(server_fd == ERR){
		fprintf(stderr, "ERROR: failed to establish a server socket\n");
		return ERR;
	}

	Pool new_pool;
	init_pool(server_fd, &new_pool);

	int client_fd;
	struct sockaddr_in clientAddress;
	socklen_t addLength = (socklen_t)(sizeof(struct sockaddr_in));

	//Initialisation of mutexes
	pthread_mutex_init(&mutex_log, NULL);;
	pthread_mutex_init(&mutex_dns, NULL);
	pthread_mutex_init(&mutex_clientfd, NULL);

	while (1) {
		new_pool.ready_read_set = new_pool.active_read_set;
	        new_pool.num_ready = select(new_pool.max_active_read_fd + 1,
					    &(new_pool.ready_read_set),
					    NULL, NULL, NULL);

		//if we detect input from listening socket
		if (FD_ISSET(server_fd, &(new_pool.ready_read_set))) {
			//STEP 1 : Accepting a new request from the client
			client_fd = accept(server_fd, 
					   (struct sockaddr *)(&clientAddress), 
					   &addLength);
			if(client_fd < 0){
				continue;
			}
			//add a new client socket
			add_client(client_fd, clientAddress, &new_pool);
		}
		//create a worker thread for added clients
		check_clients(&new_pool);
		
	}
	close(server_fd);
	
	return OK;
}
