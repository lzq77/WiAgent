
#include "controller_event.h"
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include <sys/stat.h>
#include <netinet/in.h>                         // for sockaddr_in  
#include <sys/types.h>                          // for socket  
#include <sys/socket.h>                         // for socket 

#include <stdio.h>
#include <stdlib.h>                     
#include <string.h>                             // for memset
#include <errno.h>
#include <assert.h>
#include <stdbool.h>

static int udp_fd;

static int
push(const char *data)
{
    if (data != NULL) {
       if (write(udp_fd, data, strlen(data)) < 0) {
            return -1;
       }
       return 0;
    }
    return -1;
}


/*
static int 
create_udp_socket(const char *ipv4, const int port)
{
    int sockfd;
    struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);  //tcp socket 
    if (sockfd < 0)
    {  
        perror("Create Socket Failed!\n");  
        return -1;
    }

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	if(inet_pton(servaddr.sin_family, ipv4, &servaddr.sin_addr) != 1)
	{
		perror( "Server ip address error!\n");
		return -1;
	}
	servaddr.sin_port = htons(port);
    fprintf(stderr, "servaddr.sin_port = htons(port);\n");
	if(connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr) ) < 0)
	{
		perror("UDP connect error!\n");
		return -1;
	}

	return sockfd;
}
****************/

#define PING "ping"

void
push_ping(void) 
{
    if (push(PING) > 0) {
        printf("ping controller.\n");
    }
}

void 
ping_timer(evutil_socket_t fd, short what, void *address)
{
    static bool first = true;

    if (first) {
        assert(fd > 0);
        udp_fd = fd;
        first = false;
    }
    else {
        push_ping(); 
    }
}


