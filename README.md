#Simple Multi-thread Webproxy
Simple Multi-thread Webproxy, Non-blocking I/O

##
###Preliminaries
csapp module (csapp.c & csapp.h) is borrowed from the [page](http://csapp.cs.cmu.edu/public/code.html) of Bryant and O'Hallaron's Computer Systems: A Programmer's Perspective.

##
###Simple Descriptions
- A simple web proxy using multi-thread and select to handle all clients simultaneously. 
- The proxy port by default is 12345. 
- There is a log file (using thread) to keep track of each request of cients (count client_IP:port server_IP:port client_request)

##
###Detailed Descriptions
1. How to use :
  1. Compile and run (make run). [make help for more info]
  2. Launch the client programs via socket-executable, browser, or telnet-terminal; and connect it to the proxy server which is binded to "0.0.0.0:12345".
  3. Send the HTTP request GET header terminated by "\r\n" ("enter" in telnet).
    1. "http://" may or may not be included in the GET request.
    2. the format need to be respected.(the capital letters respected and no extra spaces even)
  4. Send other HTTP request headers such as "Host:, Connection-Alive etc" but these extra headers will be ignored (and be only forwarded) by proxy server.
  5. Finish the request with an empty line terminated by "\r\n" .
  6. Check the proxy.log for details of successful connections.
    1. Unreachable webserver considered as unsuccessful connection as the proxy server will stop the job and close the connection.
    2. Connection which produces an error of "Page not found" will be considered as successful connection as the whole process succeeded but returns an error "page not found" to the client which is still a page.

2. Conception of proxy server:
  1. Proxy server will run continuosly until a control-c detected.
	2. In the main thread, it accepts initial "connect" requests from different clients 
	   in a multiplexing I/O manner and adds them to the read set.
	3. After checking the added and ready to be read clients in the main thread, 
	   we create a worker thread for each client and clear the client socket from the select pool.
	4. Every thread creates a new client socket to relay the packet to the requested web server
	   and receives the reply, before forwarding the reply to the client.
	5. In case of any error encountered in any part of the main job, thread will stop 
	   and exit immediately (without continuing the rest of the job) after closing properly the socket.
	6. In case of error encountered in logging part, this part will be skipped and the rest of
	   the job will be continued.
	7. A data structure called HttpPacket is implemented to store the http request GET header
	   from the client. (domain name can be ip address if clients initiate connections with only
	   ip address)
	8. The log file (proxy.log) is accessed by concurent proccesses in a secure way
	   using the mecanism of mutex (mutex_log).
	9. Numbering of the log file is determined by counting previous written lines as we
	   suppose that log information will never be destroyed, or else we may have
	   duplicate number for numbering.
	10. 3 mutexes are used (mutex_dns, mutex_log, mutex_client_fd) to avoid possible data races
	   between threads as information gathered from dns server can be reutilised, logging file
	   can be accessed concurrently and client file descriptor array elements can be accessed 
	   concurrently in case of reutilisation elements of pool.

3. Other elements to be considered :
  1. Codes are divided into 4 parts : 
    1. main program : contains the main thread of adding and checking clients (I/O multiplexing)
    2. tasks : worker thread for main job, logging part and other elementary functions related to pool and thread.
    3. http handler : contains all the functions and data structures related to handling http request, reply and forward.
    4. socket handler : contains all the functions to create a new server listening socket, a new client socket (for proxy to connect to web server), to write the packets onto socket and to get informations of connected client socket.
  2. Basic error / exception handlers are implemented (Unreachable web server, Web server not accepting any request, etc)
  
##
There are still bugs to be resolved, and improvements to be done.
