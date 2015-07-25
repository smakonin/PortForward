/****************************************************************************
*
* PFWD (Port Forward)
* COMP8005 -- Final Project
*
* Version 1.0
*
* Written By: 	Stephen Makonin (A00662003)
* Written On: 	March 20th, 2008
*
* Description:	Complete source for projects.
*
****************************************************************************/

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_EVENTS          1000
#define MAX_PACKET_LENGTH   65535

struct epoll_event  *events     = NULL;
int                 epoll_fd    = -1;   
char                *real_server_name;
unsigned short      real_server_port;

/****************************************************************************

FUNCTION:	    fatal_error
DATE:		    March 20th, 2008
DESIGNER: 	    Stephen Makonin (A00662003)
PROGRAMMER: 	Stephen Makonin (A00662003)
DESCRIPTION:  	Display error message and quit.

REVISIONS (Date and Description):

INTERFACE: 	    void fatal_error(const char *format, ...)
	
		        fmt:       format of the text.
		        ...:       format variables.

RETURNS:  	    n/a

****************************************************************************/
void fatal_error(const char *format, ...)
{   
	va_list varg;
	va_start(varg, format);
	vfprintf(stderr, format, varg);
	fprintf(stderr, " (%d: %s)\n\n", errno, strerror(errno));
	va_end(varg);
	exit(1);
}

/****************************************************************************

FUNCTION:	    socket_set_nonblocking
DATE:		    March 20th, 2008
DESIGNER: 	    Stephen Makonin (A00662003)
PROGRAMMER: 	Stephen Makonin (A00662003)
DESCRIPTION:  	sets up the socket to be non-blocking.

REVISIONS (Date and Description):

INTERFACE: 	    int socket_set_nonblocking(int fd)
	
		        fd:       the socket.

RETURNS:  	    -1 if error

****************************************************************************/
int socket_set_nonblocking(int fd)
{
	int flags;
	
	if((flags = fcntl(fd, F_GETFL, 0)) < 0) 
 		return -1;
 	
 	if(fcntl(fd, F_SETFL, flags | O_NDELAY) < 0) 
 		return -1;
 	
 	return 0;
}

/****************************************************************************

FUNCTION:	    setup_socket_server
DATE:		    March 20th, 2008
DESIGNER: 	    Stephen Makonin (A00662003)
PROGRAMMER: 	Stephen Makonin (A00662003)
DESCRIPTION:  	sets up the listening server socket.

REVISIONS (Date and Description):

INTERFACE: 	    int setup_socket_server(unsigned short port)
	
		        port:       port number to use.

RETURNS:  	    socket number, else -1 if error

****************************************************************************/
int setup_socket_server(unsigned short port)
{
	struct sockaddr_in  addr;
	socklen_t           addr_length;
	int                 fd, opt, opt_length;
	
	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		return -1;
	
	opt = 1;
	opt_length = sizeof(opt);
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, opt_length) == -1) 
		return -1;

    if(socket_set_nonblocking(fd) == -1)
        return -1;
		
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	
	addr_length = sizeof(addr);
	if(bind(fd, (struct sockaddr *)&addr, addr_length) < 0) 
		return -1;
	
	if(listen(fd, SOMAXCONN) < 0) 
		return -1;
		
	return fd;
}

/****************************************************************************

FUNCTION:	    setup_epoll
DATE:		    March 20th, 2008
DESIGNER: 	    Stephen Makonin (A00662003)
PROGRAMMER: 	Stephen Makonin (A00662003)
DESCRIPTION:  	sets up epoll event queue.

REVISIONS (Date and Description):

INTERFACE: 	    int setup_epoll(int size, struct epoll_event **events)
	
		        size:       number of events.
		        *events:    array of events.

RETURNS:  	    epoll fd, else -1 if error

****************************************************************************/
int setup_epoll(int size, struct epoll_event **events)
{
    int fd;

	if((*events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * size)) == NULL) 
		return -1;

	if((fd = epoll_create(size)) == -1) 
		free(*events);
	
	return fd;
}

/****************************************************************************

FUNCTION:	    add_event
DATE:		    March 20th, 2008
DESIGNER: 	    Stephen Makonin (A00662003)
PROGRAMMER: 	Stephen Makonin (A00662003)
DESCRIPTION:  	add to epoll event queue.

REVISIONS (Date and Description):

INTERFACE: 	    int add_event(int epoll_fd, int flags, int socket_fd)
	
		        epoll_fd:    the epoll fd.
		        flags:       event flags.
		        socket_fd:   the socket fd.

RETURNS:  	    -1 if error

****************************************************************************/
int add_event(int epoll_fd, int flags, int socket_fd)
{
	struct epoll_event event;
	
	event.data.fd = socket_fd;
	event.events = flags;

	return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);
}

/****************************************************************************

FUNCTION:	    delete_event
DATE:		    March 20th, 2008
DESIGNER: 	    Stephen Makonin (A00662003)
PROGRAMMER: 	Stephen Makonin (A00662003)
DESCRIPTION:  	delete from epoll event queue.

REVISIONS (Date and Description):

INTERFACE: 	    int delete_event(int epoll_fd, int socket_fd)
	
		        epoll_fd:    the epoll fd.
		        socket_fd:   the socket fd.

RETURNS:  	    -1 if error

****************************************************************************/
int delete_event(int epoll_fd, int socket_fd)
{
	return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, socket_fd, NULL);
}

/****************************************************************************

FUNCTION:	    handle_exit_signal
DATE:		    March 20th, 2008
DESIGNER: 	    Stephen Makonin (A00662003)
PROGRAMMER: 	Stephen Makonin (A00662003)
DESCRIPTION:  	if abrupt exit clean up.

REVISIONS (Date and Description):

INTERFACE: 	    void handle_exit_signal(int signo)
	
		        signo:       signal id.

RETURNS:  	    n/a

****************************************************************************/
void handle_exit_signal(int signo)
{
	free(events);	
	exit(signo);	
}

/****************************************************************************

FUNCTION:	    create_socket
DATE:		    Feb 16th, 2008
DESIGNER: 	    Stephen Makonin (A00662003)
PROGRAMMER: 	Stephen Makonin (A00662003)
DESCRIPTION:  	This function creats a connected socket to the server.

REVISIONS (Date and Description):

INTERFACE: 	    int create_socket(char *host, int port)
	
		        host:       the host name/ip to connect to.
		        port:       the port number to connect to.

RETURNS:  	    socket descriptor

****************************************************************************/
int create_socket(char *host, int port)
{
    int                 fd;
    struct sockaddr_in  server_info;
    struct hostent	    *host_ptr;
  	
    if((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;

    bzero((char *)&server_info, sizeof(struct sockaddr_in));
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(port);

    if((host_ptr = gethostbyname(host)) == NULL)
        return -1;

    bcopy(host_ptr->h_addr, (char *)&server_info.sin_addr, host_ptr->h_length);   	
   	
	// Connecting to the server
	if(connect(fd, (struct sockaddr *)&server_info, sizeof(server_info)) == -1)
		return -1;
			
	return fd;
}

/****************************************************************************

FUNCTION:	    serve_client
DATE:		    March 20th, 2008
DESIGNER: 	    Stephen Makonin (A00662003)
PROGRAMMER: 	Stephen Makonin (A00662003)
DESCRIPTION:  	service the client connection.

REVISIONS (Date and Description):

INTERFACE: 	    int serve_client(int fd)
	
		        fd:       the socket.

RETURNS:  	    -1 if error

****************************************************************************/
int serve_client(int fd, int is_oob)
{
    int	    i, n, length, server_fd, from_fd, to_fd, temp_fd;
	char	packet[MAX_PACKET_LENGTH], oob_data;
	
	
	if((server_fd = create_socket(real_server_name, real_server_port)) == -1)
	{
	    fprintf(stderr, "Unable to connect to forwaring server. (%d: %s)\n", errno, strerror(errno));	 
	    return -1;   
	}
	
	if(socket_set_nonblocking(server_fd) == -1)
	{
		fprintf(stderr, "Unable to make server soecket non-blocking, closing (socket %d).\n", server_fd);
		close(server_fd);
		return -1;
    }
    
    if(is_oob)
    {
        recv(fd, &oob_data, sizeof(oob_data), MSG_OOB);         
        send(server_fd, &oob_data, sizeof(oob_data), MSG_OOB);       
        fprintf(stderr, "OOB socket %d sent %c (fwd svr %d).\n", fd, oob_data, server_fd);       
    }
            
    from_fd = fd;
    to_fd = server_fd;
    for(i = 0; i < 2; i++)
    {
        length = MAX_PACKET_LENGTH;
        bzero(packet, length);
        n = 0;
        
        while(!n)
        {            
            if((n = recv(from_fd, packet, length, 0)) == -1)
            {
                if(errno == 11)
                {
                    n = 0;
                }
                else
                {
                    fprintf(stderr, "Unable receive from (socket %d) (%d: %s).\n", from_fd, errno, strerror(errno));
                    close(server_fd);
                    return -1;
                }
                        
            }
            
            //fprintf(stderr, "%s", packet);
            if(n)
            {
                length = n;            
                if((n = send(to_fd, packet, length, 0)) == -1)
                {
                    fprintf(stderr, "Unable send to (socket %d) (%d: %s).\n", to_fd, errno, strerror(errno));
                    close(server_fd);
                    return -1;
                }
            }
        }
        
        if(!i)
            fprintf(stderr, "Socket %d sent %d / ", from_fd, length);
        else            
            fprintf(stderr, "recv %d (fwd svr %d).\n", length, from_fd);
        
        temp_fd = from_fd;
        from_fd = to_fd;
        to_fd = temp_fd;
    }
    
    close(server_fd);    
    return 0;
}

/****************************************************************************

FUNCTION:	    worker
DATE:		    Feb 16th, 2008
DESIGNER: 	    Stephen Makonin (A00662003)
PROGRAMMER: 	Stephen Makonin (A00662003)
DESCRIPTION:  	This function is used for each thread.

REVISIONS (Date and Description):

INTERFACE: 	    void* worker(void* msg)
	
		        msg:        a pointer to the data

RETURNS:  	    pointer

****************************************************************************/
void* worker(void *msg)
{
    struct epoll_event  *event = (struct epoll_event*)msg;
    int                 fd = -1, is_oob = 0;  
    
    fd = event->data.fd;
    
    if(event->events & EPOLLPRI)
        is_oob = 1;
    
    if(serve_client(fd, is_oob) == -1)
    {
        fprintf(stderr, "Unable to serve client, closing (socket %d).\n", fd);
        delete_event(epoll_fd, fd);
    }			        

    return NULL;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in  addr;
	socklen_t           addr_length;
    int                 i, trigger_count, listen_fd, client_fd;
    unsigned short      port;
    pthread_t           thread;

    fprintf(stderr, "\n\nPFWD (Port Forward) v1.0\n\n");    

    if(argc != 4)
    {
        fprintf(stderr, "usage: pfwd [listen port] [forward host] [forward port]\n");
        fprintf(stderr, "\tlisten port:  The port pfwd sould listen on.\n");
        fprintf(stderr, "\tforward host: The forwarding server hostname/ip.\n");
        fprintf(stderr, "\tforward port: The forwarding server port.\n\n");
        exit(1);
	}
    
    port = (unsigned short)atoi(argv[1]);
    real_server_name = argv[2];
    real_server_port = (unsigned short)atoi(argv[3]);
    
	if((listen_fd = setup_socket_server(port)) == -1)
		fatal_error("Unable to setup socket.");

	if((epoll_fd = setup_epoll(MAX_EVENTS, &events)) == -1) 
		fatal_error("Unable to setup Epoll events.");
		
	if(add_event(epoll_fd, EPOLLIN | EPOLLET, listen_fd) == -1)
		fatal_error("Unable to add listening socket to Epoll event queue.");

	signal(SIGINT, handle_exit_signal);
	signal(SIGTERM, handle_exit_signal);    

    fprintf(stderr, "Resquests in port %d will be forwarded to %s:%d...\n", port, real_server_name, real_server_port);   

    addr_length = sizeof(addr);
	while(1) 
	{
		if((trigger_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1)) < 0) 
			fatal_error("Unable to Epoll wait.");
		
		for(i = 0; i < trigger_count; i++) 
		{
		    client_fd = events[i].data.fd;
		    
            if(events[i].events & (EPOLLHUP | EPOLLERR)) 
            {
		        fprintf(stderr, "Client disconnected (socket %d).\n", client_fd);
                delete_event(epoll_fd, client_fd);
            }
			else if(client_fd == listen_fd) 
			{
				if((client_fd = accept(listen_fd, (struct sockaddr *)&addr, &addr_length)) == -1) 
				{
    				fprintf(stderr, "Unable to accept new client connection.\n");
    				continue;
    			}
    				
				if(socket_set_nonblocking(client_fd) == -1)
				{
					fprintf(stderr, "Unable to make soecket non-blocking, closing (socket %d).\n", client_fd);
					close(client_fd);
					continue;
			    }

				if(add_event(epoll_fd, EPOLLIN | EPOLLPRI | EPOLLET, client_fd) == -1)
				{
					fprintf(stderr, "Unable to add soecket to Epoll event queue, closing (socket %d).\n", client_fd);
					close(client_fd);
					continue;
			    }

				fprintf(stderr, "New client connection from %s (socket %d).\n", inet_ntoa(addr.sin_addr), client_fd);
			} 
			else 
			{
                if(pthread_create(&thread, NULL, worker, (void*)&events[i]) != 0)
			    {
					fprintf(stderr, "Unable to create thread, closing (socket %d).\n", client_fd);
                    delete_event(epoll_fd, client_fd);
					continue;
			    }
			    
			    pthread_join(thread, NULL);
			}
		}
	}

    exit(0);
}
