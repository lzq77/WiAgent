/****************
 * * Helloworld.c
 * * The most simplistic C program ever written.
 * * An epileptic monkey on crack could write this code.
 * *****************/
#include <stdio.h>
#include <event2/event.h>  

struct event *ev;

static void levent_udp_timeout(evutil_socket_t fd, short what, void *arg)
{
	printf("hello timeout\n");
	struct timeval timeout;
	
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
	
	evtimer_add(ev, &timeout);//循环增加定时器

}


int main(int argc, char **argv)  
{
	struct event_base *base; //定义一个event_base  

	struct timeval timeout;
	
	base = event_base_new(); //初始化一个event_base  

	ev = evtimer_new(base, levent_udp_timeout, NULL);
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
	
	evtimer_add(ev, &timeout);

	int y = event_base_dispatch(base);
	event_base_free(base);  
	
	return 1;  
}
