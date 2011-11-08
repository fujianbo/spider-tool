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

#ifndef _SPIDER_IO_H
#define _SPIDER_IO_H


#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#ifdef HAVE_EPOLL

#include <sys/epoll.h>

#define SPD_IO_IN 	EPOLLIN		/*! Input ready */
#define SPD_IO_OUT 	EPOLLOUT	/*! Output ready */
#define SPD_IO_PRI	EPOLLPRI	/*! Priority input ready */

/* Implicitly polled for */
#define SPD_IO_ERR	EPOLLERR	/*! Error condition (errno or getsockopt) */
#define SPD_IO_HUP	EPOLLHUP	/*! Hangup */

#define SPD_IO_CONTEXT_NONE	(-1)

typedef int spd_io_context_t;

struct spd_io_rec;

/*
 * A spider IO callback takes its io_rec, a file descriptor, list of events, and
 * callback data as arguments and returns 0 if it should not be
 * run again, or non-zero if it should be run again.
 */
typedef int (*spd_io_cb)(struct spd_io_rec *ior, int fd, short events, void *cbdata);

struct spd_io_rec {
	spd_io_cb callback;
	void *data;
	int fd;
};

static inline void spd_io_init(struct spd_io_rec *ior, spd_io_cb callback, void *data)
{
	ior->callback = callback;
	ior->data = data;
	ior->fd = -1;
}

#define spd_ioactive(rec) ((rec)->fd != -1)

/*! Creates a context
 *
 * Create a context for I/O operations
 *
 * \param slots	number of slots (file descriptors) to allocate initially (the number grows as necessary)
 *
 * \return the created I/O context
 */

#define spd_io_context_create(slots) epoll_create(slots)

/*! Destroys a context
 *
 * \param ioc	context to destroy
 *
 * Destroy an I/O context and release any associated memory
 */
#define spd_io_context_destroy(ioc) close(ioc)
 

/*! Adds an IO context
 *
 * \param ioc		context to use
 * \param fd		fd to monitor
 * \param events	events to wait for
 *
 * \return 0 on success or -1 on failure
 */
#define spd_io_add(ioc, ior,filedesc, mask) ({ \
	const typeof(ior) __ior = (ior); \
	const typeof(filedesc) __filedesc = (filedesc); \
	struct epoll_event ev = {         \
		.events = (mask), \
		.data = {.ptr = __ior},      \
	};       \
	int ret; \
	\
	if(!(ret = epoll_ctl((ioc), EPOLL_CTL_ADD, __filedesc, &ev))) \
		ior->fd = __filedesc; \
	ret;     \
})

/*! Removes an IO context
 *
 * \param ioc	io_context to remove it from
 * \param ior	io_rec to remove
 *
 * \return 0 on success or -1 on failure.
 */
 #define spd_io_remove(ioc, ior) ({ \
	struct epoll_event ev;  \
	const typeof(ior) __ior = (ior);  \
	int ret;  \
	\
	if(!(ret = epoll_ctl((ioc), EPOLL_CTL_DEL, __ior->fd, &ev))) \
		ior->fd = -1; \
	ret; \
 })

/*! Waits for IO
 *
 * \param ioc		context to act upon
 * \param howlong	how many milliseconds to wait
 *
 * Wait for I/O to happen, returning after
 * howlong milliseconds, and after processing
 * any necessary I/O.  Returns the number of
 * I/O events which took place.
 */
 int spd_io_wait(spd_io_context_t ioc, int howlong);
 
#else  /* HAVE_EPOLL */

/*! Input ready */
#define SPD_IO_IN         POLLIN
/*! Output ready */
#define SPD_IO_OUT        POLLOUT
/*! Priority input ready */
#define SPD_IO_PRI        POLLPRI

/* Implicitly polled for */
/*! Error condition (errno or getsockopt) */
#define SPD_IO_ERR        POLLERR
/*! Hangup */
#define SPD_IO_HUP        POLLHUP
/*! Invalid fd */
#define SPD_IO_NVAL       POLLNVAL

typedef int (*spd_io_cb)(int *id, int fd, short events, void *data);
#define SPD_IO_CB(a) (a)

/*! \brief
 * Kept for each file descriptor
 */
struct io_rec {
	spd_io_cb io_callback;   /*!< What is to be called */
	void *data;              /*!< Data to be passed */
	int *id;                 /*!< ID number */
};

/*! \brief
 * An spider IO callback takes its id, a file descriptor, list of events, and
 * callback data as arguments and returns 0 if it should not be
 * run again, or non-zero if it should be run again.
 */

/* These two arrays are keyed with
   the same index.  it's too bad that
   pollfd doesn't have a callback field
   or something like that.  They grow as
   needed, by GROW_SHRINK_SIZE structures
   at once */

#define GROW_SHRINK_SIZE 512

/*! \brief Global IO variables are now in a struct in order to be
   made threadsafe */
struct io_context {
	struct pollfd *fds;      /*!< Poll structure */
	struct io_rec *ior;      /*!< Associated I/O records */
	unsigned int fdcnt;      /*!< First available fd */
	unsigned int maxfdcnt;   /*!< Maximum available fd */
	int current_ioc;         /*!< Currently used io callback */
	int needshrink;          /*!< Whether something has been deleted */
};

/*! 
 * \brief Creates a context 
 * Create a context for I/O operations
 * Basically mallocs an IO structure and sets up some default values.
 * \return an allocated io_context structure
 */
struct io_context *spd_io_context_create(void);

/*! 
 * \brief Destroys a context 
 * \param ioc structure to destroy
 * Destroy a context for I/O operations
 * Frees all memory associated with the given io_context structure along with the structure itself
 */
 void spd_io_context_destroy(struct io_context *ioc);

 /*! 
 * \brief Adds an IO context 
 * \param ioc which context to use
 * \param fd which fd to monitor
 * \param callback callback function to run
 * \param events event mask of events to wait for
 * \param data data to pass to the callback
 * Watch for any of revents activites on fd, calling callback with data as
 * callback data.  
 * \retval a pointer to ID of the IO event
 * \retval NULL on failure
 */
int *spd_io_context_add(struct io_context *ioc, int fd, spd_io_cb iocb, void *data, short events);

 /*! 
 * \brief Changes an IO handler 
 * \param ioc which context to use
 * \param id
 * \param fd the fd you wish it to contain now
 * \param callback new callback function
 * \param events event mask to wait for
 * \param data data to pass to the callback function
 * Change an I/O handler, updating fd if > -1, callback if non-null, 
 * and revents if >-1, and data if non-null.
 * \retval a pointer to the ID of the IO event
 * \retval NULL on failure
 */
int *spd_io_context_change(struct io_context *ioc, int *id, int fd, spd_io_cb iocb, void *data, short events);

/*! 
 * \brief Removes an IO context 
 * \param ioc which io_context to remove it from
 * \param id which ID to remove
 * Remove an I/O id from consideration  
 * \retval 0 on success
 * \retval -1 on failure
 */
int spd_io_context_remove(struct io_context *ioc, int *id);

/*! 
 * \brief Waits for IO 
 * \param ioc which context to act upon
 * \param howlong how many milliseconds to wait
 * Wait for I/O to happen, returning after
 * howlong milliseconds, and after processing
 * any necessary I/O.  
 * \return he number of I/O events which took place.
 */
int spd_io_context_wait(struct io_context *ioc, int howlong);

/*! 
 * \brief Dumps the IO array.
 * Debugging: Dump everything in the I/O array
 */
void spd_io_context_dump(struct io_context *ioc);

#endif /* HAVE_EPOLL */

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
