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

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <sys/types.h>
#include <errno.h>

#include "thread.h"
#include "logger.h"

int spd_pthread_create_stack(pthread_t *thread, pthread_attr_t *attr, void*(*start_routine)(void*), 
			void *data, size_t stacksize, const char *file, const char *caller, int line, const char *start_fn)
{
	if(!attr) {
		attr = alloca(sizeof(*attr));
		pthread_attr_init(attr);
	}

	/* On Linux, pthread_attr_init() defaults to PTHREAD_EXPLICIT_SCHED,
	   which is kind of useless. Change this here to
	   PTHREAD_INHERIT_SCHED; that way the -p option to set realtime
	   priority will propagate down to new threads by default.
	   This does mean that callers cannot set a different priority using
	   PTHREAD_EXPLICIT_SCHED in the attr argument; instead they must set
	   the priority afterwards with pthread_setschedparam(). */
	   if((errno = pthread_attr_setinheritsched(attr, PTHREAD_INHERIT_SCHED)))
	       spd_log(LOG_WARNING, "pthread_attr_setinheritsched: %d \n", strerror(errno));

	   if(!stacksize)
	   	stacksize = SPD_STACKSIZE;

	   if((errno = pthread_attr_setstacksize(attr, stacksize ? stacksize: SPD_STACKSIZE)))
	   	spd_log(LOG_WARNING, "pthread_attr_setstacksize %d", strerror(errno));

	   return pthread_create(thread, attr, start_routine, data);
}

int spd_pthread_create_detached_stack(pthread_t *thread, pthread_attr_t *attr, void*(*start_routine)(void*), 
			void *data, size_t stacksize, const char *file, const char *caller, int line, const char *start_fn)
{
	unsigned int need_destroy = 0;
	int res;
	
	if(!attr) {
		attr = alloca(sizeof(*attr));
		pthread_attr_init(attr);
		need_destroy = 1;
	}

	if ((errno = pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED)))
		spd_log(LOG_WARNING, "pthread_attr_setdetachstate: %d\n", strerror(errno));

	res = spd_pthread_create_stack(thread, attr, start_routine, data, stacksize, file,
			caller, line, start_fn);

	if(need_destroy)
		pthread_attr_destroy(attr);

	return res;
}

void spd_thread_sleep(uint64_t ms)
{
	struct timespec interval;

	interval.tv_sec = (long)(ms/1000);
	interval.tv_nsec = (long)(ms%1000) * 1000000;

	nanosleep(&interval, 0);
}

