/*
 * Spider -- An open source C language toolkit.
 *
 * Copyright (C) 2011 , Inc.
 *
 * lidp <openser@yeah.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */


#include<stdio.h>  
#include<unistd.h>  
#include<sys/types.h>  
#include<stdlib.h>  
#include<sys/socket.h>  
#include<errno.h>  
#include<fcntl.h>  
#include<netinet/in.h>  
#include<sys/resource.h>

#include "logger.h"
#include "fdevent.h"  

//foward declaration  
static fdnode *fdnode_init();  
static void fdnode_free(fdnode *fdn);  
  
  
fdevents *fdevent_init(size_t maxfds){  
    fdevents *ev;  
  
    ev = spd_calloc(1,sizeof(*ev));  
    ev->fdarray = spd_calloc(maxfds , sizeof(*ev->fdarray));  
    ev->maxfds = maxfds;  
      
    ev->epoll_fd = epoll_create(maxfds);  
    if(-1==ev->epoll_fd){  
        spd_log(LOG_ERROR,"epoll create failed,%s\n",strerror(errno));  
        return NULL;  
    }  
  
    ev->epoll_events = spd_malloc(maxfds * sizeof(*ev->epoll_events));  
  
    return ev;  
}  
void fdevent_free(fdevents *ev){  
    size_t i;  
    if(!ev) return ;  
  
    close(ev->epoll_fd);  
    spd_safe_free(ev->epoll_events);  
      
    for(i=0; i<ev->maxfds; i++){  
        if(ev->fdarray[i]) spd_safe_free(ev->fdarray[i]);        
    }  
  
    spd_safe_free(ev->fdarray);  
    spd_safe_free(ev);  
  
}  
  
int fdevent_register(fdevents *ev,int fd,fdevent_handler handler,void *ctx){  
    fdnode *fdn;  
    fdn=fdnode_init();  
    fdn->handler=handler;  
    fdn->fd=fd;  
    fdn->ctx=ctx;  
  
    ev->fdarray[fd]=fdn;  
  
    return 0;  
}  
  
int fdevent_unregister(fdevents *ev,int fd){  
    if(!ev) return 0;  
  
    fdnode *fdn=ev->fdarray[fd];  
    fdnode_free(fdn);  
    ev->fdarray[fd]=NULL;  
  
    return 0;  
}  
  
int fdevent_event_add(fdevents *ev,int fd,int events){  
    struct epoll_event ep;  
    int add = 0;  
  
    fdnode *fdn=ev->fdarray[fd];  
    add = (fdn->status == -1 ? 1 : 0);  
      
    memset(&ep,0,sizeof(ep));  
    ep.events = 0;  
  
    if(events & FDEVENT_IN) ep.events |=EPOLLIN;  
    if(events & FDEVENT_OUT) ep.events |=EPOLLOUT;  

  	/**
	 *
	 * with EPOLLET we don't get a FDEVENT_HUP
	 * if the close is delay after everything has
	 * sent.
	 *
	 */

	ep.events |= EPOLLERR | EPOLLHUP /* | EPOLLET */;
	
    ep.data.ptr=NULL;  
    ep.data.fd=fd;  
  
    if(0 != epoll_ctl(ev->epoll_fd,add ? EPOLL_CTL_ADD : EPOLL_CTL_MOD,fd,&ep)){  
        spd_log(LOG_ERROR,"epoll_ctl failed %s\n",strerror(errno));  
        return -1;  
    }  
      
    if(add){  
        fdn->status=1;  
    }  
    return 0;  
  
}  
int fdevent_event_del(fdevents *ev,int fd){  
    struct epoll_event ep;  
    fdnode *fdn = ev->fdarray[fd];   
    if(-1 == fdn->status){  
        return 0;  
    }  
    memset(&ep,0,sizeof(ep));  
  
    ep.data.ptr = NULL;  
    ep.data.fd = fd;  
  
    if(0 != epoll_ctl(ev->epoll_fd,EPOLL_CTL_DEL,fd,&ep)){  
        spd_log(LOG_ERROR,"epoll del failed %s\n",strerror(errno));  
        return -1;  
    }  
  
    fdn->status = -1;  
    return 0;  
}  
int fdevent_loop(fdevents *ev, int timeout_ms){  
    int n,i;  
    for(;;){  
        n=0;  
        if((n=epoll_wait(ev->epoll_fd,ev->epoll_events,ev->maxfds,timeout_ms))>0){  
            for(i=0;i<n;i++){  
                fdevent_handler handler;  
                void *context;  
                int events = 0, e,fd;  
                fdnode *fdn;  
                  
                e=ev->epoll_events[i].events;  
                if (e & EPOLLIN) events |= FDEVENT_IN;  
                if (e & EPOLLOUT) events |= FDEVENT_OUT;  
                  
                fd=ev->epoll_events[i].data.fd;  
  
                fdn = ev->fdarray[fd];  
                context = fdn->ctx;  
                fdn->handler(fd,context,events);  
            }     
        }  
    }  
}  

static fdnode *fdnode_init(){  
    fdnode *fdn;  
  
    fdn = spd_calloc(1,sizeof(*fdn));  
    fdn->fd=-1;  
    fdn->status=-1;  
  
    return fdn;  
}  

static void fdnode_free(fdnode *fdn){  
    spd_safe_free(fdn);  
}
static int fdevent_fcntl_set(fdevents *ev, int fd)
{
	#ifdef FD_CLOEXEC
	/* close fd on exec (cgi) */
	fcntl(sock->fd, F_SETFD, FD_CLOEXEC);

	#ifdef O_NONBLOCK
	return fcntl(sock->fd, F_SETFL, O_NONBLOCK | O_RDWR);
}

