#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>

#define HANDLE_INFO   1
#define HANDLE_SEND   2
#define HANDLE_DEL    3
#define HANDLE_CLOSE  4

#define MAX_REQLEN          1024
#define MAX_PROCESS_CONN    3
#define FIN_CHAR            0x00
#define SUCCESS  0
#define ERROR   -1

#define tinyhttp_conf  "/etc/tinyhttp/tinyhttp.conf"
#define DEFAULT_PORT   8080
#define DEFAULT_MAX_FDS  1024

/* server signal */
static volatile sig_atomic_t is_shutdown = 0;

typedef struct event_handle{
    int socket_fd;
    int file_fd;
    int file_pos;
    int epoll_fd;
    char request[MAX_REQLEN];
    int request_len;
    int ( * read_handle )( struct event_handle * ev );
    int ( * write_handle )( struct event_handle * ev );
    int handle_method;
} EV,* EH;

typedef int ( * EVENT_HANDLE )( struct event_handle * ev );

int create_accept_fd( int listen_fd ){

	int addr_len = sizeof( struct spd_sockaddr);
    struct spd_sockaddr remote_addr;

	int accept_fd = spd_accept(listen_fd, &remote_addr);
	
    int flags = fcntl( accept_fd, F_GETFL, 0 );
    fcntl( accept_fd, F_SETFL, flags|O_NONBLOCK );

	return accept_fd;
}

int init_evhandle(EH ev,int socket_fd,int epoll_fd,EVENT_HANDLE r_handle,EVENT_HANDLE w_handle){
    ev->epoll_fd = epoll_fd;
    ev->socket_fd = socket_fd;
    ev->read_handle = r_handle;
    ev->write_handle = w_handle;
    ev->file_pos = 0;
    ev->request_len = 0;
    ev->handle_method = 0;
    memset( ev->request, 0, 1024 );
}
//accept->accept_queue->request->request_queue->output->output_queue
//multi process sendfile
int parse_request(EH ev){
    ev->request_len--;
    *( ev->request + ev->request_len - 1 ) = 0x00;
    int i;
    for( i=0; i<ev->request_len; i++ ){
        if( ev->request[i] == ':' ){
            ev->request_len = ev->request_len-i-1;
            char temp[MAX_REQLEN];
            memcpy( temp, ev->request, i );
            ev->handle_method = atoi( temp );
            memcpy( temp, ev->request+i+1, ev->request_len );
            memcpy( ev->request, temp, ev->request_len );
            break;
        }
    }
    //handle_request( ev );
    //register to epoll EPOLLOUT

    struct epoll_event ev_temp;
    ev_temp.data.ptr = ev;
    ev_temp.events = EPOLLOUT|EPOLLET;
    epoll_ctl( ev->epoll_fd, EPOLL_CTL_MOD, ev->socket_fd, &ev_temp );
    return SUCCESS;
}

int handle_request(EH ev){
    struct stat file_info;
    switch( ev->handle_method ){
        case HANDLE_INFO:
            ev->file_fd = open( ev->request, O_RDONLY );
            if( ev->file_fd == -1 ){
                send( ev->socket_fd, "open file failed\n", strlen("open file failed\n"), 0 );
                return -1;
            }
            fstat(ev->file_fd, &file_info);
            char info[MAX_REQLEN];
            sprintf(info,"file len:%d\n",file_info.st_size);
            send( ev->socket_fd, info, strlen( info ), 0 );
            break;
        case HANDLE_SEND:
            ev->file_fd = open( ev->request, O_RDONLY );
            if( ev->file_fd == -1 ){
                send( ev->socket_fd, "open file failed\n", strlen("open file failed\n"), 0 );
                return -1;
            }
            fstat(ev->file_fd, &file_info);
            sendfile( ev->socket_fd, ev->file_fd, 0, file_info.st_size );
            break;
        case HANDLE_DEL:
            break;
        case HANDLE_CLOSE:
            break;
    }
    finish_request( ev );
    return SUCCESS;
}

int finish_request(EH ev){
    close(ev->socket_fd);
    close(ev->file_fd);
    ev->handle_method = -1;
    clean_request( ev );
    return SUCCESS;
}

int clean_request(EH ev){
    memset( ev->request, 0, MAX_REQLEN );
    ev->request_len = 0;
}

int read_hook( EH ev ){
    char in_buf[MAX_REQLEN];
    memset( in_buf, 0, MAX_REQLEN );
    int recv_num = recv( ev->socket_fd, &in_buf, MAX_REQLEN, 0 );
    if( recv_num ==0 ){
        close( ev->socket_fd );
        return ERROR;
    }
    else{
        //check ifoverflow
        if( ev->request_len > MAX_REQLEN-recv_num ){
            close( ev->socket_fd );
            clean_request( ev );
        }
        memcpy( ev->request + ev->request_len, in_buf, recv_num );
        ev->request_len += recv_num;
        if( recv_num == 2 && ( !memcmp( &in_buf[recv_num-2], "\r\n", 2 ) ) ){
            parse_request(ev);
        }
    }
    return recv_num;
}

int write_hook(EH ev){
    struct stat file_info;
    ev->file_fd = open( ev->request, O_RDONLY );
    if( ev->file_fd == ERROR ){
        send( ev->socket_fd, "open file failed\n", strlen("open file failed\n"), 0 );
        return ERROR;
    }
    fstat(ev->file_fd, &file_info);
    int write_num;
    while(1){
        write_num = sendfile( ev->socket_fd, ev->file_fd, (off_t *)&ev->file_pos, 10240 );
        ev->file_pos += write_num;
        if( write_num == ERROR ){
            if( errno == EAGAIN ){
                break;
            }
        }
        else if( write_num == 0 ){
            printf( "writed:%d\n", ev->file_pos );
            //finish_request( ev );
            break;
        }
    }
    return SUCCESS;
}

static int init_network(struct spd_sockaddr *addr)
{
	int server_fd = -1;
	int x = 1;
	int flags;
	
	if(spd_sockaddr_isnull(addr)) {
		spd_log(LOG_DEBUG, "server is disabled\n");
		return -1;
	}

	server_fd = socket(spd_sockaddr_is_ipv6(addr) ? 
						AF_INET6 : AF_INET, SOCK_STREAM, 0);

	if(server_fd < 0) {
		spd_log(LOG_ERROR, "Unable to alloca server fd %s\n", strerror(errno));
		return -1;
	}

	setsockopt(server_fd, SOL_SOCEK, SO_REUSEADDR, &x, sizeof(x));

	if(spd_bind(server_fd, addr)) {
		spd_log(LOG_ERROR, "Uable bind http server to %s  : %s\n", 
					spd_sockaddr_tostring_default(addr), strerror(errno));
		goto error;
	}

	if(spd_listen(server_fd, 10)) {
		spd_log(LOG_ERROR, "Uable listen on %s : %s\n",
					spd_sockaddr_tostring_default(addr), strerror(errno));
		goto error;
	}

	flags = fcntl(server_fd, F_GETFL);
	fcntl(server_fd, F_SETFL,flags | O_NONBLOCK);
	fcntl(server_fd, F_SETFD, FD_CLOEXEC);
	
	return server_fd;
	
	error :
		close(server_fd);
		server_fd = -1;
		return server_fd;
}

static void __init_tinyhttp()
{
	struct spd_config *cfg;
	struct spd_variable *v;
	int server_fd = -1;

	struct spd_sockaddr *server_addr = NULL;
	uint32_t bindport = DEFAULT_PORT;
	int num_addrs = 0;
	int max_workers = 0;
	int max_fds = DEFAULT_MAX_FDS;

	cfg = spd_config_load(tinyhttp_conf);
	if(!cfg) {
		spd_log(LOG_WARNING, "Unble to open config file %s\n", tinyhttp_conf);
		return;
	}

	v = spd_variable_browse(v->name,"general");

	for(; v; v = v->next) {
		if(!strcasecmp(v->name, "bindport")) {
			bindport = atoi(v->value);
		} else if(!strcasecmp(v->name, "bindaddr")) {
			if(!(num_addrs = spd_sockaddr_resolve(&server_addr, v->value, 0, SPD_AF_UNSPEC))) {
				spd_log(LOG_WARNING, "Invalid bind address %s\n", v->value);
			} else {
				spd_log(LOG_NOTICE, "Got %d addresses\n", num_addrs);
			}
		} else if(!strcasecmp(v->name, "max_worker")) {
			max_workers = atoi(v->value);
		} else if(!strcasecmp(v->name, "max_fd")) {
			max_fds = atoi(v->value);
		} else {
			spd_log(LOG_WARNING, "Ignore unkown options %s in %s\n", v->name, tinyhttp_conf);
		}
	}

	spd_config_destroy(cfg);

	if(!spd_sockaddr_get_port(server_addr)) {
		spd_sockaddr_set_port(server_addr, bindport);
	}

	/* prepare  socket */
	server_fd = init_network(server_addr);

	if(server_fd < 0) {
		spd_log(LOG_ERROR, "failed init network\n");
		return;
	}
	
    /* start watcher and workers  */
	if(max_workers > 0) {
		int is_child = 0;
		while(!is_child && !is_shutdown) {
			if(max_workers > 0) {
				switch (fork()) {
					case -1:
						return -1;
					case 0:
						is_child = 1;
						break;
					default :
						max_workers--;
						break;
				}
			} else {
				int status;
				if(wait(&status) != -1) {
					max_workers++;
				} else {
					switch(errno) {
						case INTR:
							break;
						default :
							spd_log(LOG_ERROR, "failed wait %s\n", strerror(errno));
							break;
					}
				}
			}
		}
		if(is_shutdown) {
			/* kill all childs */
			kill(0, SIGTERM);
		}
		if(!is_child)
			return 0;
	}

	/* prepare fd event */
	int accept_handles = 0;
    struct epoll_event ev, events[max_fds];
    int epfd = epoll_create(max_fds);
    int ev_s = 0;

    ev.data.fd = listen_fd;
    ev.events = EPOLLIN|EPOLLET;
    epoll_ctl( epfd, EPOLL_CTL_ADD, listen_fd, &ev );
    struct event_handle ev_handles[256];
	
    for( ;; ){
        ev_s = epoll_wait( epfd, events, max_fds, 1000);
        int i = 0;
        for( i = 0; i<ev_s; i++ ){
            if(events[i].data.fd == server_fd){
                if(accept_handles < MAX_PROCESS_CONN){
                        accept_handles++;
                        int accept_fd = create_accept_fd(server_fd);
                        init_evhandle(&ev_handles[accept_handles],accept_fd,epfd,read_hook,write_hook);
                        ev.data.ptr = &ev_handles[accept_handles];
                        ev.events = EPOLLIN|EPOLLET;
                        epoll_ctl( epfd, EPOLL_CTL_ADD, accept_fd, &ev );
                    }
                }
                else if( events[i].events&EPOLLIN ){
                    EVENT_HANDLE current_handle = ( ( EH )( events[i].data.ptr ) )->read_handle;
                    EH current_event = ( EH )( events[i].data.ptr );
                    ( *current_handle )( current_event );
                }
                else if( events[i].events&EPOLLOUT ){
                    EVENT_HANDLE current_handle = ( ( EH )( events[i].data.ptr ) )->write_handle;
                    EH current_event = ( EH )( events[i].data.ptr );
                    if( ( *current_handle )( current_event )  == 0 ){
                        accept_handles--;
                    }
                }
            }
        }
}

void init_tinyhttp()
{
	__init_tinyhttp();
}

