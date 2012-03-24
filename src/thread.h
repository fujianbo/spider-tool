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

#ifndef _SPIDER_THREAD_H
#define _SPIDER_THREAD_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" { 
#endif


#include <pthread.h>

/* default thread stacksize is 240 KB */
#define SPD_BACKGROUND_STACKSIZE          (((sizeof(void*) *8*8) - 16) * 1024)
#define SPD_STACKSIZE                                 (((sizeof(void*) *8*8) - 16) * 1024)

int spd_pthread_create_stack(pthread_t *thread, pthread_attr_t *attr, void*(*start_routine)(void*), 
			void *data, size_t stacksize, const char *file, const char *caller, int line, const char *start_fn);

int spd_pthread_create_detached_stack(pthread_t *thread, pthread_attr_t *attr, void*(*start_routine)(void*), 
			void *data, size_t stacksize, const char *file, const char *caller, int line, const char *start_fn);

/*
 *\breif  yeild thread for xx ms
 */
void spd_thread_sleep(int ms);

/* usage :
  pthread_t thread; 
  pthread_attr_t attr;
  
  pthread_attr_init(&attr);
  spd_pthread_create(&thread, &attr, bridge_call_thread, data);
  pthread_attr_destroy(&attr);
 */
#define spd_pthread_create(a,b,c,d)      \
	spd_pthread_create_stack(a,b,c,d,  \
		0,__FILE__, __FUNCTION__, __LINE__, #c)

/* usage :
    spd_pthread_create_detached(&threadid, NULL, start_routine, parame);
    most time you give attr to NULL, and do not need desotry attr
  */
#define spd_pthread_detached(a,b,c,d)       \
	spd_pthread_create_detached_stack(a,b,c,d, \
		0, __FILE_, __FUNCTION__, __LINE__, #c)

/*same with spd_pthread_create except the stacksize */
#define spd_pthread_create_background(a, b, c, d)	\
	spd_pthread_create_stack(a, b, c, d,		\
		SPD_BACKGROUND_STACKSIZE,			\
		__FILE__, __FUNCTION__, __LINE__, #c)

/*the same with spd_pthread_create_background except the stacksize */
#define spd_pthread_create_detached_background(a, b, c, d)	\
	spd_pthread_create_detached_stack(a, b, c, d,		\
		SPD_BACKGROUND_STACKSIZE,			\
		__FILE__, __FUNCTION__, __LINE__, #c)


#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
