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
 * \file taskqueue.h
 * \brief An API for managing task processing threads that can be shared across modules
 *
 *
 * \note A taskqueue is a named singleton containing a processing thread and
 * a task queue that serializes tasks pushed into it by [a] module(s) that reference the taskqueue.  
 * A  taskqueue is created the first time its name is requested via the spd_taskqueue_get()
 * function and destroyed when the taskqueue reference count reaches zero.
 *
 * Modules that obtain a reference to a taskqueue can queue tasks into the taskqueue
 * to be processed by the singleton processing thread when the task is popped off the front 
 * of the queue.  A task is a wrapper around a task-handling function pointer and a data
 * pointer.  It is the responsibility of the task handling function to free memory allocated for
 * the task data pointer.  A task is pushed into a taskqueue queue using the 
 * spd_taskqueue_push(taskqueue, taskhandler, taskdata) function and freed by the
 * taskqueue after the task handling function returns.  A module releases its reference to a
 * taskqueue using the spd_taskqueue_unreference() function which may result in the
 * destruction of the taskprocessor if the taskqueue's reference count reaches zero.  Tasks waiting
 * to be processed in the taskqueue queue when the taskqueue reference count reaches zero
 * will be purged and released from the taskqueue queue without being processed.
 */
 
#ifndef _SPIDER_TASKQUEUE_H
#define _SPIDER_TASKQUEUE_H


struct spd_taskqueue;

/*!
 * \brief spd_tps_options for specification of c options
 *
 * Specify whether a v should be created via spd_taskqueue_get() if the taskqueue 
 * does not already exist.  The default behavior is to create a taskqueue if it does not already exist 
 * and provide its reference to the calling function.  To only return a reference to a taskqueue if 
 * and only if it exists, use the TPS_REF_IF_EXISTS option in spd_taskqueue_get().
 */
enum spd_tps_options {
	/*! \brief return a reference to a v, create one if it does not exist */
	TPS_REF_DEFAULT = 0,
	/*! \brief return a reference to a taskqueue ONLY if it already exists */
	TPS_REF_IF_EXISTS = (1<< 0),
};

/*!
 * \brief Get a reference to a taskprocessor with the specified name and create the taskprocessor if necessary
 *
 * The default behavior of instantiating a taskprocessor if one does not already exist can be
 * disabled by specifying the TPS_REF_IF_EXISTS spd_tps_options as the second argument to spd_taskprocessor_get().
 * \param name The name of the taskprocessor
 * \param create Use 0 by default or specify TPS_REF_IF_EXISTS to return NULL if the taskprocessor does 
 * not already exist
 * return A pointer to a reference counted taskprocessor under normal conditions, or NULL if the
 * TPS_REF_IF_EXISTS reference type is specified and the taskprocessor does not exist
 */
struct spd_taskqueue * spd_taskqueue_get(const char *name, enum spd_tps_options option);

/*!
 * \brief Unreference the specified taskprocessor and its reference count will decrement.
 *
 * Taskprocessors use obj and will unlink from the taskprocessor singleton container and destroy
 * themself when the taskprocessor reference count reaches zero.
 * \param tps taskprocessor to unreference
 * \return NULL
 */

void *spd_taskqueue_unreference(struct spd_taskqueue *tps);

/*!
 * \brief Push a task into the specified taskprocessor queue and signal the taskprocessor thread
 * \param tps The taskprocessor structure
 * \param task_exe The task handling function to push into the taskprocessor queue
 * \param datap The data to be used by the task handling function
 * \retval 0 success
 * \retval -1 failure
 * \since 1.6.1
 */
 int spd_taskqueue_push(struct spd_taskqueue *tps, int(*task_exe)(void *data),void *data);

/*!
 * \brief Return the name of the taskprocessor singleton
 */
const char *spd_taskqueue_name(struct spd_taskqueue *tps);

/* initialize the taskprocessor container */
int spd_taskqueue_init(void);


#endif