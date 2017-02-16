#include "tasks.h"

/*
 * Initialisation of socket pool
 */
void init_pool(int listen_fd, Pool *p)
{
	//Initialisation of informations for select
	FD_ZERO(&(p->active_read_set));
	FD_ZERO(&(p->ready_read_set));
	FD_SET(listen_fd, &(p->active_read_set));
	p->max_active_read_fd = listen_fd;
	
	//Initialisation of all client fd
	//by default, negative number for non-connected socket
	int i;
	for(i=0; i<FD_SETSIZE; i++){
		p->clientfd[i]=-1;
	}

	p->maxi = -1;
	p->num_ready = 0;
}

/*
 * Add a new client to the Pool
 */
void add_client(int connfd, struct sockaddr_in clientAddress, Pool *p)
{
	p->num_ready--;

	// Find an available slot
	int i;
	for (i = 0; i < FD_SETSIZE; i++){
		pthread_mutex_lock(&mutex_clientfd);
		if (p->clientfd[i] < 0) {
			pthread_mutex_unlock(&mutex_clientfd);

			// Add connected descriptor to the pool
			p->clientfd[i] = connfd;
			p->clientAddress[i] = clientAddress;

			//Add the descriptor to active read descriptor set
			FD_SET(connfd, &(p->active_read_set));

			// Update max descriptor
			if (connfd > p->max_active_read_fd){
				p->max_active_read_fd = connfd;
			}
			if (i > p->maxi){
				p->maxi = i;
			}
			break;
		}
		pthread_mutex_unlock(&mutex_clientfd);
	}
	// Couldn't find an empty slot
	if (i == FD_SETSIZE){
		fprintf(stderr, "ERROR: add_client error: Too many clients\n");
	}
}

/* 
 * Check added clients and create worker thread for them
 */
void check_clients(Pool *p) 
{
	int i, connfd;
	pthread_t tid;
	for (i = 0; (i <= p->maxi) && (p->num_ready > 0); i++) {
		pthread_mutex_lock(&mutex_clientfd);
		connfd = p->clientfd[i];
		pthread_mutex_unlock(&mutex_clientfd);
	        //If we haven't create a worker thread yet for the client
		if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_read_set))) {
			//We remove the client fd from the active read set as
			//we have create a worker thread
			p->num_ready--;
			FD_CLR(connfd, &p->active_read_set);
			//Creation of worker thread with arguments
			(thread_args[i]).client_fd = connfd;
			(thread_args[i]).clientAddress = p->clientAddress[i];
			(thread_args[i]).client_fd_ptradr = &(p->clientfd[i]);
			if(pthread_create(&tid, NULL, main_task, 
					  (void *)(&(thread_args[i])))){
				fprintf(stderr, "ERROR: creation of thread \
failed\n");
				p->clientfd[i] = -1;
				close(connfd);
				return;
			}
			//Remove all used resources of the thread once finished
			pthread_detach(tid);
		}
	}
}

/*
 * Close and put negative number to the client fd
 */
static void close_client_fd(int fd, int *fd_ptradr)
{
	close(fd);
	pthread_mutex_lock(&mutex_clientfd);
	*fd_ptradr = -1;
	pthread_mutex_unlock(&mutex_clientfd);
}

/*
 * The main task for the worker thread - handling each client request
 */
void *main_task(void *thread_args)
{
	//Main arguments for the main task
	int client_fd = ((sthread *)thread_args)->client_fd;
	struct sockaddr_in clientAddress = ((sthread *)thread_args)->
		clientAddress;
	int *client_fd_ptradr = ((sthread *)thread_args)->client_fd_ptradr;

	//STEP 2 : Receiving client HTTP request
	HttpPacket *packet;
	char *remainingPacket;
	if(get_http_request(client_fd, &packet, 
			    &remainingPacket) == ERR){
		fprintf(stderr, "ERROR: failed to receive HTTP request. \
Retry from the beginning!\n");
		close_client_fd(client_fd, client_fd_ptradr);
		return NULL;
	}

	//STEP 3 : Creating a new client socket for forwarding
	int proxy_fd;
	proxy_fd = server_forwarding_request(packet);
	if(proxy_fd == ERR){
		fprintf(stderr, "ERROR: failed to establish a client socket \
for forwarding. Retry from the beginning!\n");
		free_http_packet(packet);
		if(remainingPacket != NULL){
			free(remainingPacket);
		}
		close_client_fd(client_fd, client_fd_ptradr);
		return NULL;
	}

	//STEP 4 : Forwarding request
	if(forward_http_request(proxy_fd, packet, 
				remainingPacket) == ERR){
		fprintf(stderr, "ERROR: failed to forward request. \
Retry from the beginning!\n");
		free_http_packet(packet);
		if(remainingPacket != NULL){
			free(remainingPacket);
		}
		close_client_fd(client_fd, client_fd_ptradr);
		close(proxy_fd);
		return NULL;
	}
	if(remainingPacket != NULL){
		free(remainingPacket);
	}

	//STEP 5 : Receiving and forwarding reply
	if(get_forward_http_reply(proxy_fd, client_fd) == ERR){
		fprintf(stderr, "ERROR: failed to get and forward reply. \
Retry from the beginning!\n");
		free_http_packet(packet);
		close_client_fd(client_fd, client_fd_ptradr);
		close(proxy_fd);
		return NULL;
	}

	//STEP 6 : Getting info and Logging
	//Get client info
	char *client_ipAddress;
	int client_portNumber=0;
	if(get_connected_client_info(&clientAddress, 
				     &client_ipAddress, 
				     &client_portNumber) == OK){

		//Do the logging process
		if(logging(client_ipAddress, client_portNumber,
			   packet)== ERR){
			fprintf(stderr, "ERROR: logging failed for the current \
request!\n");
		}

	}else{
		fprintf(stderr, "ERROR: logging failed for the current \
request!\n");
	}

	//STEP 7 : Free and close all the related sockets
	free_http_packet(packet);
	close_client_fd(client_fd, client_fd_ptradr);
	close(proxy_fd);
	return NULL;
}

/*
 * Logging for Proxy Server
 */
int logging(char *clientIpAdd, int clientPortNum, HttpPacket *packet)
{
	if(packet == NULL)
	{
		fprintf(stderr, "ERROR: zero information to log\n");
		return ERR;
	}

	int file_fd = open("proxy.log", O_CREAT | O_RDWR, 0777);
	if(file_fd == -1){
		fprintf(stderr, "ERROR: failed to create/open proxy.log\n");
		return ERR;
	}

	pthread_mutex_lock(&mutex_log);

	//----------------- START LOCKING AREA ----------------------------//

	//To find the numbering
	unsigned int line_number = 0;
	unsigned int number_of_lines = 0;
	char readCharacter;
	int readByte;
	while((readByte = read(file_fd, &readCharacter, 1))!=0) {
		if(readByte==1 && readCharacter=='\n'){
			number_of_lines++;
		}
	}
	line_number = number_of_lines + 1;
	
	//Converting line number to string format, max line number is a number
	//with 100 digits
	char line_number_string[100];
	memset(line_number_string, 0, 100);
	sprintf(line_number_string, "%u", line_number);

	/*
	 * Writing the information of the request
	 */
	if((line_number_string == NULL) | (clientIpAdd == NULL) || 
	   (packet == NULL) || (packet->ipAddress == NULL) || 
	   (packet->portNum == NULL) || (packet->action == NULL) || 
	   (packet->page == NULL)){
		fprintf(stderr, "ERROR: failed to read info for logging\n");
		return ERR;
	}
	//Numbering & client ip address
	write(file_fd, line_number_string, strlen(line_number_string));
	write(file_fd, " ", 1); 
	write(file_fd, clientIpAdd, strlen(clientIpAdd)); 
	write(file_fd, ":", 1); 
	//Client port number (from int to string)
	char client_portNum_string[6];
	memset(client_portNum_string, 0, 6);
	sprintf(client_portNum_string, "%u", clientPortNum);
	write(file_fd, client_portNum_string, strlen(client_portNum_string));
	//Server & action info
	write(file_fd, " ", 1); 
	write(file_fd, packet->ipAddress, strlen(packet->ipAddress));
	write(file_fd, ":", 1); 
	write(file_fd, packet->portNum, strlen(packet->portNum));
	write(file_fd, " ", 1); 
	write(file_fd, packet->action, strlen(packet->action)); 
	write(file_fd, " ", 1); 
	write(file_fd, packet->page, strlen(packet->page));
	write(file_fd, "\n", 1);
	
	//----------------- END LOCKING AREA ----------------------------//

	pthread_mutex_unlock(&mutex_log);

	close(file_fd);

	return OK;
}
