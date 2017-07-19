/****************
 * * Helloworld.c
 * * The most simplistic C program ever written.
 * * An epileptic monkey on crack could write this code.
 * *****************/
#include "controller_event.h"
#include "controller_push.h"
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

#define PUSH_PORT 2819
#define CONTROL_PORT 6777


int main(int argc, char **argv)
{
	struct event_base *base; //定义一个event_base  
    struct event *ev_timer;
	struct timeval two_sec;
	char *controller_ip;
    unsigned int controller_port;
    struct evconnlistener *control_listener;
	struct sockaddr_in sin;
    struct sockaddr_in push_addr;
    int push_sock;

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
    
	memset(&push_addr, 0, sizeof(push_addr));
	push_addr.sin_family = AF_INET;
	if(inet_pton(push_addr.sin_family, controller_ip, &push_addr.sin_addr) != 1)
	{
		perror( "controller_ip address error!\n");
		return -1;
	}
	push_addr.sin_port = htons(PUSH_PORT);

    push_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (push_sock < 0)
    {
        perror("Create Socket Failed!\n");  
        return -1;
    }
    if(connect(push_sock, (struct sockaddr*)&push_addr, sizeof(push_addr) ) < 0)
	{
		perror("UDP connect error!\n");
		return -1;
	}

    ev_timer = event_new(base, push_sock, EV_TIMEOUT | EV_PERSIST, 
            ping_timer, NULL);
	two_sec.tv_sec = 2;
    two_sec.tv_usec = 0;
	event_add(ev_timer, &two_sec);

	event_base_dispatch(base);
	event_base_free(base);  
	
	return 0;
}
