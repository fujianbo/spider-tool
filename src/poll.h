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

#ifndef _SPIDER_POLL_H
#define _SPIDER_POLL_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include<sys/poll.h>

#ifndef SPD_INSTERNAL_POLL

#define spd_poll(a,b,c)  poll(a,b,c)

#else

#define POLLIN		0x01
#define POLLPRI		0x02
#define POLLOUT		0x04
#define POLLERR		0x08
#define POLLHUP		0x10
#define POLLNVAL	0x20

#define spd_poll(a,b,c)  spd_internal_poll(a,b,c)

int spd_internal_poll(struct pollfd *fds, unsigned long nfds, int timeout);

#endif


int spd_waitfor_pollin(int fd, int ms);

int spd_waitfor_pollout(int fd, int ms);


#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
