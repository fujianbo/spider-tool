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


#include <unistd.h>				 /* standard Unix definitions */
#include <sys/types.h>					   /* system types */
#include <sys/time.h>						/* time definitions */
#include <assert.h>						  /* assertion macros */
#include <string.h>						  /* string functions */
#include <errno.h>
#include "poll.h"
#include "logger.h"
#include "time.h"

#ifdef SPD_INTERNAL_POLL

int spd_internal_poll(struct pollfd *fds, unsigned long nfds, int timeout)
{
	
}

#endif

int spd_waitfor_pollin(int fd, int ms)
{
	struct pollfd pfd[1];
	
	memset(pfd, 0, sizeof(pfd));

	pfd[0].fd = fd;
	pfd[0].events = POLLIN || POLLPRI;

	return spd_poll(pfd, 1, ms);
}

int spd_waitfor_pollout(int fd, int ms)
{
	struct pollfd fds[1];
	int ret;
	int escap= 0;
	struct timeval start = spd_tvnow();
	
	fds[0].fd = fd;
	fds[0].events = POLLOUT;

	while((ret = spd_poll(fds, 1, ms - escap)) <= 0) {
		if(ret == 0) {
			/* timeout */
			return -1;
		} else if( ret == -1) {
			if(errno == EINTR || errno == EAGAIN) {
				escap = spd_tvdiff_ms(spd_tvnow(), start);
				if(escap > ms) {
					return -1;
				}
				/* this is a accept error , continue wait */
				continue;
			}
			/* Fatal error, bail. */
			spd_log(LOG_ERROR, "poll returned error: %s\n", strerror(errno));
			return -1;
		} else {
			escap = spd_tvdiff_ms(spd_tvnow, start);
			if(escap >= ms) {
				return -1;
			}
		}
	}

	return 0;
}