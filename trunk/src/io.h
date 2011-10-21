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

#include "poll.h"


#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

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

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
