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

/*!
 * \brief General Spider locking definitions.
 *
 *
 * This file provides different implementation of the functions,
 * depending on the platform, the use of DEBUG_THREADS, and the way
 * module-level mutexes are initialized.
 *
 *  - \b static: the mutex is assigned the value SPD_MUTEX_INIT_VALUE
 *        this is done at compile time, and is the way used on Linux.
 *        This method is not applicable to all platforms e.g. when the
 *        initialization needs that some code is run.
 *
 *  - \b through constructors: for each mutex, a constructor function is
 *        defined, which then runs when the program (or the module)
 *        starts. The problem with this approach is that there is a
 *        lot of code duplication (a new block of code is created for
 *        each mutex). Also, it does not prevent a user from declaring
 *        a global mutex without going through the wrapper macros,
 *        so sane programming practices are still required.
 */

#ifndef _SPIDER_LOCK_H
#define _SPIDER_LOCK_H

#include <pthread.h>
#include <netdb.h>
#include <time.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define SPD_PTHREADT_NULL (pthread_t) -1
#define SPD_PTHREADT_STOP (pthread_t) -2

#define PTHREAD_MUTEX_INIT_VALUE	PTHREAD_MUTEX_INITIALIZER
#define SPD_MUTEX_KIND			PTHREAD_MUTEX_RECURSIVE_NP

#ifdef DEBUG_THREADS

#define SPD_MUTEX_INIT_VALUE { PTHREAD_MUTEX_INIT_VALUE, 1, { NULL }, { 0 }, 0, { NULL } \
, { 0 }, PTHREAD_MUTEX_INIT_VALUE }
#define SPD_MUTEX_INIT_VALUE_NOTRACKING \
                             { PTHREAD_MUTEX_INIT_VALUE, 0, { NULL }, { 0 }, 0, { NULL } \
, { 0 }, PTHREAD_MUTEX_INIT_VALUE }


#define SPD_MAX_REENTRANCY 10

struct spd_mutex_info {
    pthread_mutex_t mutex;
    /*! Track which thread holds this lock */
    unsigned int track:1;
    const char *file[SPD_MAX_REENTRANCY];
    int lineno[SPD_MAX_REENTRANCY];
    int reentrancy;
    const char * func[SPD_MAX_REENTRANCY];
    pthread_t thread[SPD_MAX_REENTRANCY];
    pthread_mutex_t reenter_mutex;
};

typedef struct spd_mutex_info spd_mutex_t;

typedef pthread_cond_t spd_cond_t;

typedef pthread_mutex_t empty_mutex;

enum spd_lock_type {
    SPD_MUTEX,
    SPD_RDLOCK,
    SPD_WRLOCK,
};

/*!
 * \brief Store lock info for the current thread
 *
 * This function gets called in spd_mutex_lock() and spd_mutex_trylock() so
 * that information about this lock can be stored in this thread's
 * lock info struct.  The lock is marked as pending as the thread is waiting
 * on the lock.  spd_mark_lock_acquired() will mark it as held by this thread.
 */

void spd_store_lock_info(enum spd_lock_type type, const char *name, int linenum,
    const char *func, const char *lock_name, const char *lock_addr);

/*!
 * \brief Mark the last lock as acquired
 */
void spd_mark_lock_acquired(void *lock_addr);

/*!
 * \brief Mark the last lock as failed (trylock)
 */
void spd_mark_lock_failed(void *lock_addr);

/*!
 * \brief remove lock info for the current thread
 *
 * this gets called by spd_mutex_unlock so that information on the lock can
 * be removed from the current thread's lock info struct.
 */
void spd_remove_lock_info(void *lock_addr);

/*!
 * \brief retrieve lock info for the specified mutex
 *
 * this gets called during deadlock avoidance, so that the information may
 * be preserved as to what location originally acquired the lock.
 */
int spd_find_lock_info(void *lock_addr, char *filename, size_t filename_size, int *
    lineno, char *func, size_t func_size, char *mutex_name, size_t mutex_name_size);

/*!
 * \brief Unlock a lock briefly
 *
 * used during deadlock avoidance, to preserve the original location where
 * a lock was originally acquired.
 */
 #define DEADLOCK_AVOIDANCE(lock) \
    do { \
        char __filename[80], __func[80], __mutex_name[80]; \
        int __lineno; \
        int res2, __res = spd_find_lock_info(lock, __filename, \
        sizeof(__filename),&__lineno, __func, sizeof(__func), __mutex_name, sizeof(__mutex_name));\
        __res2 = spd_mutex_unlock(lock); \
        usleep(1); \
        if(__res < 0) { \
            if(__res2 == 0) { \
                spd_mutex_lock(lock); \
            } else { \
                ; \
            } \
        } else { \
            if(__res2 == 0) { \
                __spd_pthread_mutex_lock(__filename, __lineno, __func, __mutex_name, lock); \
            } else { \
                ; \
            } \
          } while (0) 


static void __attibute__((constructor)) init_empty_mutex(void)
{
    memset(&empty_mutex, 0, sizeof(empty_mutex));
}



#else /* !DEBUG_THREADS */


#define	DEADLOCK_AVOIDANCE(lock) \
	do { \
		int __res; \
		if (!(__res = spd_mutex_unlock(lock))) { \
			usleep(1); \
			spd_mutex_lock(lock); \
		} else { \
			; \
		} \
	} while (0)

/* pthread库 互斥锁封装  */
typedef pthread_mutex_t spd_mutex_t; 

#define SPD_MUTEX_INIT_VALUE ((spd_mutex_t) PTHREAD_MUTEX_INIT_VALUE)
#define SPD_MUTEX_INIT_VALUE_NOTRACKING \
    ((spd_mutex_t) PTHREAD_MUTEX_INIT_VALUE

#define spd_mutex_ini_notracking(m) spd_mutex_int(m)

static inline spd_mutex_init(spd_mutex_t *spdmutex)
{
    int res;
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, SPD_MUTEX_KIND);

    res = pthread_mutex_init(spdmutex, &attr);
    pthread_mutex_attr_destroy(&attr);
    return res;
}

#define spd_pthread_mutex_init(pmutex, a) pthread_mutex_init(pmutex,a)

static inline int spd_mutex_unlock(spd_mutex_t *pmutex)
{
    return pthread_mutex_unlock(pmutex);
}

static inline int spd_mutex_destroy(spd_mutex_t *pmutex)
{
    return pthread_mutex_destroy(pmutex);
}

static inline int spd_mutex_lock(spd_mutex_t *pmutex)
{
    return pthread_mutex_lock(pmutex);
}

static inline int spd_mutex_trylock(spd_mutex_t *pmutex)
{
    return pthread_mutex_trylock(pmutex);
}

/* pthread库 条件变量封装  */
typedef pthread_cond_t spd_cond_t;

static inline int spd_cond_init(spd_cond_t *cond, pthread_condattr_t *cond_attr)
{
    return pthread_cond_init(cond, cond_attr);
}


/* 发送一个信号给另外一个正在处于阻塞等待状态的线程,使其脱离阻塞状态,
 *  继续执行.如果没有线程处在阻塞等待状态,pthread_cond_signal也会成功返回。
 */
static inline int spd_cond_signal(spd_cond_t *cond)
{
    return pthread_cond_signal(cond);
}

/* */
static inline int spd_cond_broadcast(spd_cond_t *cond)
{
    return pthread_cond_broadcast(cond);
}

static inline int spd_cond_destroy(spd_cond_t *cond)
{
    return pthread_cond_destroy(cond);
}

static inline int spd_cond_wait(spd_cond_t *cond, spd_mutex_t *pmutex)
{
    return pthread_cond_wait(cond, pmutex);
}

/* */
static inline int spd_cond_timewait(spd_cond_t *cond, spd_mutex_t *t, const struct 
    timespec *abstime)
{
    return pthread_cond_timewait(cond, t, abstime);
}

#endif /* !DEBUG_THREADS */

#define __SPD_MUTEX_DEFINE(scop, mutex, init_val, track) \
    scop spd_mutex_t mutex = init_val


#define SPD_MUTEX_DEFINE_STATIC(mutex) __SPD_MUTEX_DEFINE(static, mutex, SPD_MUTEX_INIT_VALUE, 1)
#define SPD_MUTEX_DEFINE_STATIC_NOTRAKING(mutex) __SPD_MUTEX_DEFINE(static, mutex, SPD_MUTEX_INIT_VALUE_NOTRACKING, 0)


/* pthread库读写锁封装  */

typedef pthread_rwlock_t spd_rwlock_t;

#ifdef HAVE_PTHREAD_RWLOCK_INITIALIZER
#define SPD_RWLOCK_INIT_VALUE PTHREAD_RWLOCK_INITIALIZER
#else
#define SPD_RWLOCK_INIT_VALUE NULL
#endif

#ifdef DEBUG_THREADS

#define spd_rwlock_init(rwlock) __spd_rwlock_init(__FILE__, __LINE__, __PRETTY_FUNCTION__, #rwlock, rwlock)

static inline int __spd_rwlock_init(const char *filename, int lineno, const char *func, 
    const char *rwlock_name, ast_rwlock_t *prwlock)
{
    int res;
    pthread_rwlockattr_t attr;

    pthread_rwlockattr_init(&attr);

    res = pthread_rwlock_init(prwlock, &attr);
    pthread_rwlockattr_destroy(&attr);
    return res;
}

#else /* !DEBUG_THREADS */

static inline int spd_rwlock_init(spd_rwlock_t *prwlock)
{
    int res;
    pthread_rwlockattr_t attr;

    pthread_rwlockattr_init(&attr);
    res = pthread_rwlock_init(prwlock, &attr);
    pthread_rwlockattr_destroy(&attr);
    return res;
}

static inline int spd_rwlock_destroy(spd_rwlock_t *prwlock)
{
    return pthread_rwlock_destroy(prwlock);
}

static inline int spd_rwlock_unlock(spd_rwlock_t *prwlock)
{
    return pthread_rwlock_unlock(prwlock);
}

static inline int spd_rwlock_rdlock(spd_rwlock_t *prwlock)
{
    return pthread_rwlock_rdlock(prwlock);
}

static inline int spd_rwlock_timedrdlock(spd_rwlock_t *prwlock, const struct timespec *abs_timeout)
{
    int res;
    res = pthread_rwlock_timedrdlock(prwlock, abs_timeout);
    return res;
}

static inline int spd_rwlock_tryrdlock(spd_rwlock_t *prwlock)
{
    return pthread_rwlock_tryrdlock(prwlock);
}

static inline int spd_rwlock_wrlock(spd_rwlock_t *prwlock)
{
    return pthread_rwlock_wrlock(prwlock);
}


static inline int spd_rwlock_timedwrlock(spd_rwlock_t *prwlock, const struct timespec *abs_timeout)
{
    int res;
    res = pthread_rwlock_timedwrlock(prwlock, abs_timeout);
    return res;
}

static inline int spd_rwlock_trywrlock(spd_rwlock_t *prwlock)
{
    return pthread_rwlock_trywrlock(prwlock);
}

#endif /* !DEBUG_THREADS */

#define __SPD_RWLOCK_DEFINE(scop, rwlock) \
    scop spd_rwlock_t rwlock = SPD_RWLOCK_INIT_VALUE
    
#define SPD_RWLOCK_DEFINE_STATIC(rwlock) __SPD_RWLOCK_DEFINE(static, rwlock)

#endif 

