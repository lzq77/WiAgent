/****************
 * * Helloworld.c
 * * The most simplistic C program ever written.
 * * An epileptic monkey on crack could write this code.
 * *****************/
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

struct bufferevent *bev;

static void 
levent_udp_timeout(evutil_socket_t fd, short what, void *arg)
{ 
	struct timeval timeout;
	
    if (write(fd, arg, sizeof(arg)) < 0) {
        perror("udp write");
        return;
    }
    else {
        printf("sockfd(%d),%s\n", fd, (char*)arg);
    }

    timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	evtimer_add(ev, &timeout); //loop to add timer
}

static void 
levent_control_accept(evutil_socket_t fd, short what, void *arg)
{
    int connfd = accept(fd, NULL, NULL);
    printf("Accept controller connect!\n");
    /*
     * struct event_base *base = (struct event_base *)arg;
     * struct event *ev_chatter = event_new(base, sockfd, EV_TIMEOUT, 
            levent_udp_timeout, "ping");
       event_add(ev_chatter);
     */
    
}


int 
wi_tcp_socket(const char *ipv4, const int port)
{
	/*set a local client socket address struct*/
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);  //tcp socket 
    if (sockfd < 0)
    {  
        perror("Create Socket Failed!\n");  
        return -1;  
    }

	/*Set socket server infomation*/ 
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	if(inet_pton(servaddr.sin_family, ipv4, &servaddr.sin_addr) != 1)
	{
		perror( "Server ip address error!\n");
		return -1;
	}
	servaddr.sin_port = htons(port);
	if(connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr) ) < 0)
	{
		perror("Cannot connect to server ip!\n");
		return -1;
	}

	return sockfd;
}

int 
wi_tcp_server(const int port) 
{
    // create a stream socket server
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);  
    if (listenfd < 0)  
    {  
        printf("Create Socket Failed!\n");  
        return -1;  
    }  
  
    struct sockaddr_in servaddr;  
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;  
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);  
    servaddr.sin_port = htons(port);  
    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)))  
    {  
        printf("Server Bind Port: %d Failed!\n", port);  
        return -1;  
    }  
  
    // server_socket用于监听  
    if (listen(listenfd, 20))  
    {  
        printf("Server Listen Failed!\n");  
        return -1;  
    }  

    return listenfd;
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

static void
control_readcb(struct bufferevent *bev, void *data)
{
    char *read_line;
    size_t len;
    printf("control_readcb: ");

    struct evbuffer *input = bufferevent_get_input(bev);
    while(1) {
        read_line = evbuffer_readln(input, &len, EVBUFFER_EOL_CRLF);
        if(read_line == NULL) {
            free(read_line);
            break;
        }
        else {
            printf("control_readcb evbuffer_readln read_lien=%s\n", read_line);
        }
        free(read_line);
    }
}

static void
control_writecb(struct bufferevent *bev, void *user_data)
{
    printf("control_writecb: ");
	struct evbuffer *output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
		printf("flushed answer\n");
	}
}

static void
control_eventcb(struct bufferevent *bev, short events, void *user_data)
{
    printf("control_eventcb: ");
	if (events & BEV_EVENT_EOF) {
		printf("Connection closed.\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",
		    strerror(errno));/*XXX win32*/
	}
	/* None of the other events can happen here, since we haven't enabled
	 * timeouts */
	bufferevent_free(bev);
}

static void
control_listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *sa, int socklen, void *user_data)
{
	struct event_base *base = user_data;

    printf("control_listener_cb: ");
	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(base);
		return;
	}
	bufferevent_setcb(bev, control_readcb, control_writecb, control_eventcb, NULL);
	bufferevent_enable(bev, EV_WRITE | EV_READ);
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

	control_listener = evconnlistener_new_bind(base, control_listener_cb, (void *)base,
	    LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
	    (struct sockaddr*)&sin, sizeof(sin));
    
    if (!control_listener) {
		fprintf(stderr, "Could not create a listener!\n");
		return 1;
	}
    
    //Timing send a ping message to controller.
    int ping_fd = wi_udp_socket(controller_ip, PING_PORT);
    ev = event_new(base, ping_fd, EV_TIMEOUT, 
            levent_udp_timeout, "ping");
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	evtimer_add(ev, &timeout);

	int y = event_base_dispatch(base);
	event_base_free(base);  
	
	return 0;
}
