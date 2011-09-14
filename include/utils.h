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

#ifndef _SPIDER_UTILS_H
#define _SPIDER_UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <arpa/inet.h>	/* we want to override inet_ntoa */
#include <netdb.h>
#include <limits.h>
#include <time.h>	/* we want to override localtime_r */
#include <unistd.h>

#include "logger.h"


/***********spider memory wraper functions ***************/

/*!
  * \brief safe free to prevent Wild pointer
  */
#define spd_safe_free(it) if(it) {free(it);it=NULL;}

/* msg for XXlloc failed  */
#define SPD_XXLLOC_FAIL_MSG \
     spd_log(LOG_ERROR, "Memory Alloc fail in function %s at line %d of %s\n",func, lineno, file);

/*!
  * A wrapper for glibc malloc()
  * spd_malloc() is a wrapper for malloc which while generate a log msg in case alloc 
  * fails.
  */
 #define spd_malloc(len)                \
     _spd_malloc((len), __FILE__, __LINE__, __PRETTY_FUNCTION__)

static inline void * _spd_malloc(size_t len, const char *file, int lineno, const char *func)
{
    void *ptr = NULL;

    if(!(ptr = malloc(len)))
        SPD_XXLLOC_FAIL_MSG;

    return ptr;
}

 /*!
 * \brief A wrapper for calloc()
 *
 * spd_calloc() is a wrapper for calloc() that will generate an Asterisk log
 * message in the case that the allocation fails.
 */
 #define spd_calloc(value, len)  \
    _spd_calloc((value), (len), __FILE__, __LINE__, __PRETTY_FUNCTION__)

static inline void * _spd_calloc(size_t value, size_t len, const char *file, int lineno, const char *func)
{
    void *ptr = NULL;

    if(!(ptr = calloc(value, len)))
        SPD_XXLLOC_FAIL_MSG;

    return ptr;
}

/*!
 * \brief A wrapper for realloc()
 *
 * spd_realloc() is a wrapper for realloc() that will generate an spd log
 * message in the case that the allocation fails.
 *
 * The arguments and return value are the same as realloc()
 */
#define spd_realloc(p, len) \
    _spd_realloc((p), (len), __FILE__, __LINE__, __PRETTY_FUNCTION__)

static inline void *_spd_realloc(void *p, size_t len, const char *file, int lineno, const char *func)
{
    void *newp = NULL;

    if(!(newp = realloc(p, len)))
        SPD_XXLLOC_FAIL_MSG;

    return newp;
}

/************spider memory wraper functions end  *******/



/***********spider  flags , use in struct  ***************/


struct spd_flags {
    unsigned int flags;
};

extern unsigned int __unsigned_int_flags_dummy;

#define spd_test_flag(p, flag) ({ \
        typeof ((p)->flags) __p = (p)->flags;  \
        typeof ((p)->flags) (__unsigned_int_flags_dummy) __x = 0; \
        (void) (&__p == &__x);   \
        ((p)->flags &(flag));   \
})

#define spd_set_flag(p, flag) do { \
    typeof((p)->flags) __p = (p)->flags;   \
    typeof(__unsigned_int_flags_dummy) __x = 0; \
    (void) (&__p == &__x);                     \
    ((p)->flags |= (flag));                   \
} while(0)

#define spd_clear_flag(p, flag) do { \
    typeof((p)->flags) __p = (p)->flags;   \
    typeof(__unsigned_int_flags_dummy) __x = 0; \
    (void) (&__p == &__x); \
    ((p)->flags &= ~(flag));    \
} while(0)

#define spd_copy_flags(dest,src,flagz)	do { \
	typeof ((dest)->flags) __d = (dest)->flags; \
	typeof ((src)->flags) __s = (src)->flags; \
	typeof (__unsigned_int_flags_dummy) __x = 0; \
	(void) (&__d == &__x); \
	(void) (&__s == &__x); \
	(dest)->flags &= ~(flagz); \
	(dest)->flags |= ((src)->flags & (flagz)); \
} while (0)

#define spd_set2_flag(p,value,flag)	do { \
	typeof ((p)->flags) __p = (p)->flags; \
	typeof (__unsigned_int_flags_dummy) __x = 0; \
	(void) (&__p == &__x); \
	if (value) \
		(p)->flags |= (flag); \
	else \
		(p)->flags &= ~(flag); \
} while (0)


#define spd_set_flags_to(p,flag,value)	do { \
    typeof ((p)->flags) __p = (p)->flags; \
    typeof (__unsigned_int_flags_dummy) __x = 0; \
    (void) (&__p == &__x); \
    (p)->flags &= ~(flag); \
    (p)->flags |= (value); \
} while (0)


/************ spider flags end  ********************************/


#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

/*!
 * random func wrapper 
 */
long int spd_random(void);

/*!
 * Determine if a string containing a boolean value is "true".
 * This function checks to see whether a string passed to it is an indication of an "
 *   true" value.  It checks to see if the string is "yes", "true", "y", "t", "on" or "1".  
 *
 * Returns 0 if val is a NULL pointer, -1 if "true", and 0 otherwise.
 */
int spd_true(const char *s);

/*! Make sure something is false 
 * Determine if a string containing a boolean value is "false".
 * This function checks to see whether a string passed to it is an indication of an "
 * false" value.  It checks to see if the string is "no", "false", "n", "f", "off" or "0".  
 *
 * Returns 0 if val is a NULL pointer, -1 if "false", and 0 otherwise.
 */
int spd_false(const char *s);

/*!
 * \brief wrapper func for mkdir 
 **/
 int spd_mkdir(const char *path, int mode);

#endif