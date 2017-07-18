/****************
 * * Helloworld.c
 * * The most simplistic C program ever written.
 * * An epileptic monkey on crack could write this code.
 * *****************/
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

struct event *ev;
#define PING_PORT 2819
#define CONTROL_PORT 6777

static void 
levent_udp_timeout(evutil_socket_t fd, short what, void *arg)
{	
    if (write(fd, arg, sizeof(arg)) < 0) {
        perror("udp write");
        return;
    }
    else {
        printf("sockfd(%d),%s\n", fd, (char*)arg);
    }
}

int 
wi_udp_socket(const char *ipv4, const int port)
{
    int sockfd;
    struct sockaddr_in servaddr;

    fprintf(stderr, "in the method wi_udp_socket()\n");

    /*set a local client socket address struct*/
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);  //tcp socket 
    if (sockfd < 0)
    {  
        perror("Create Socket Failed!\n");  
        return -1;
    }

	/*Set socket server infomation*/ 
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

int main(int argc, char **argv)
{
	struct event_base *base; //定义一个event_base  
	struct timeval timeout;
	char *controller_ip;
    unsigned int controller_port;
    struct evconnlistener *control_listener;
	struct sockaddr_in sin;

    if ((controller_ip = *(++argv)) == NULL) {
        fprintf(stderr, "Need controller's ip address.");
        return 1;
    }
    
    base = event_base_new();
	if (!base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}
    
    memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(CONTROL_PORT);

    /**
     * Listening controller connection, which is 
     * used to send its control commands.
     */
	control_listener = evconnlistener_new_bind(base, control_listener_cb, (void *)base,
	    LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
	    (struct sockaddr*)&sin, sizeof(sin));
    
    if (!control_listener) {
		fprintf(stderr, "Could not create a listener!\n");
		return 1;
	}
    
    /*
     * Timing send a ping message to controller.
     */
    int ping_fd = wi_udp_socket(controller_ip, PING_PORT);
    ev = event_new(base, ping_fd, EV_TIMEOUT | EV_READ | EV_PERSIST, 
            levent_udp_timeout, "ping");
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	event_add(ev, &timeout);

	event_base_dispatch(base);
	event_base_free(base);  
	
	return 0;
}
