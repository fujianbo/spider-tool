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

#include <sys/epoll.h>  
  
#define BV(x) (1 << x)  
#define FDEVENT_IN     BV(0)  
#define FDEVENT_PRI    BV(1)  
#define FDEVENT_OUT    BV(2)  
#define FDEVENT_ERR    BV(3)  
#define FDEVENT_HUP    BV(4)  
#define FDEVENT_NVAL   BV(5)  
  
typedef void (*fdevent_handler)(int fd,void *ctx, int revents);  
typedef struct _fdnode{  
    fdevent_handler handler;  
    int fd;  
    void *ctx;  
    int status;  
}fdnode;  
  
typedef struct fdevents{  
    fdnode **fdarray;  
    size_t maxfds;  
  
    int epoll_fd;  
    struct epoll_event *epoll_events;  
}fdevents;  
  
fdevents *fdevent_init(size_t maxfds);  
void fdevent_free(fdevents *ev);  
int fdevent_register(fdevents *ev,int fd,fdevent_handler handler,void *ctx);  
int fdevent_unregister(fdevents *ev,int fd);  
int fdevent_event_add(fdevents *ev,int fd,int events);  
int fdevent_event_del(fdevents *ev,int fd);  
int fdevent_loop(fdevents *ev, int timeout_ms);  