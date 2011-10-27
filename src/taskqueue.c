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
 * \file
 * \brief Maintain a container of uniquely-named taskprocessor threads that can be shared across modules.
 */
 
 #include "obj.h"
 #include "taskqueue.h"
 #include "logger.h"
 #include "utils.h"
 #include "strings.h"
 #include "thread.h"

/*!
  *\brief tps_task struct is queued to a taskqueue
  * tps_task are processed in FIFO order and freed by the taskprocessing
  * thread after task handler returns. the callback func that is assigned to the execute()
  * func pointpor is resposible for releasing data resource if necessary.
  */
 struct tps_task {
	/*! \brief The execute() task callback function pointer */
	int(*execute)(void *data);
	/*! \brief The data pointer for the task execute() function */
	void *data;
	/*! \brief SPD_LIST_ENTRY overhead */
	SPD_LIST_ENTRY(tps_task)list;
 };

/*! \brief tps_taskprocessor_stats maintain statistics for a taskprocessor. */
struct spd_taskqueue_stats {
	/*! \brief This is the maximum number of tasks queued at any one time */
	unsigned long max_qsize;
	/*! \brief the number of task processed untile now */
	unsigned long task_processed_count;
};

struct spd_taskqueue {
	/*! \brief Friendly name of the taskprocessor */
	const char *name;
	/*! \brief Thread poll condition */
	spd_cond_t poll_cond;
	/*! \brief Taskprocessor thread */
	pthread_t poll_thread;
	/*! \brief Taskprocessor lock */
	spd_mutex_t tskq_lock;
	/*! \brief Taskprocesor thread run flag */
	unsigned char poll_thread_run;
	/*! \brief Taskprocessor statistics */
	struct spd_taskqueue_stats *stats;
	/*! \brief Taskprocessor current queue size */
	long tps_queue_size;
	/*! \brief Taskprocessor queue */
	SPD_LIST_HEAD_NOLOCK(tps_queue, tps_task)tps_queue;
	/*! \brief Taskprocessor singleton list entry */
	SPD_LIST_ENTRY(spd_taskqueue)list;
};

#define TPS_MAX_BUCKETS      7

/*! \brief tps_singletons is the obj container for taskprocessor singletons */
static struct obj_container *tps_singletons;

/*! \brief The obj hash callback for taskprocessors */
static int tps_hash_cb(const void *obj, const int flags);
/*! \brief The obj compare callback for taskprocessors */
static int tps_cmp_cb(void *obj, void *arg, int flags);

/*! \brief The task processing function executed by a taskprocessor */
static void *tps_work_thread(void *data);

/*! \brief Destroy the taskprocessor when its refcount reaches zero */
static void tps_taskqueue_destroy(void *tps);

/*! \brief Remove the front task off the taskprocessor queue */
static struct tps_task *tps_taskqueue_pop(struct spd_taskqueue *tps);

static int tps_taskqueue_depth(struct spd_taskqueue *tps);

/* initialize the taskprocessor container */
int spd_taskqueue_init(void)
{
	if(!(tps_singletons = obj_container_alloc(TPS_MAX_BUCKETS, tps_hash_cb, tps_cmp_cb))) {
		spd_log(LOG_ERROR, "spd taskqueue container failed to init! \n");
		return -1;
	}

	return 0;
}

/* alloc a task, if failed ,return NULL */
static struct tps_task* tps_task_alloc(int(*exe_rutine)(void *data), void *data)
{
	struct tps_task *task;

	if((task = spd_calloc(1, sizeof(*task)))) {
		task->execute = exe_rutine;
		task->data = data;
	}

	return task;
}

/* destroy task resource */
static void * tps_task_free(struct tps_task *task)
{
	if(task) {
		spd_safe_free(task);
	}
	return NULL;
}

/* this is the work thread responsable for process task queue */
static void *tps_work_thread(void * data)
{
	struct spd_taskqueue *tps = data;
	struct tps_task *task;
	int size;

	if(!tps) {
		spd_log(LOG_ERROR, "cannot start thread func without a spd taskqueue\n");
		return NULL;
	}

	while(tps->poll_thread_run) {
		spd_mutex_lock(&tps->tskq_lock);
		if(!tps->poll_thread_run) {
			spd_mutex_unlock(&tps->tskq_lock);
			break;
		}

		if(!(size = tps_taskqueue_depth(tps))) {
			spd_cond_wait(&tps->poll_cond, &tps->tskq_lock);
			if(!tps->poll_thread_run) {
				spd_mutex_unlock(&tps->tskq_lock);
				break;
			}
		}

		spd_mutex_unlock(&tps->tskq_lock);

		
		if(!(task = tps_taskqueue_pop(tps))) {
			spd_log(LOG_ERROR, "wtf ? %d tasks in the queue ,but we pop blank tasks \n", size);
			continue;
		}

		task->execute(task->data);

		spd_mutex_lock(&tps->tskq_lock);
		if(tps->stats) {
			tps->stats->task_processed_count++;
			if(size > tps->stats->max_qsize) {
				tps->stats->max_qsize = size;
			}
		}
		spd_mutex_unlock(&tps->tskq_lock);

		tps_task_free(task);
		
	}

	while((task = tps_taskqueue_pop(tps))) {
		tps_task_free(task);
	}

	return NULL;
		
}

static int tps_hash_cb(const void * obj, const int flags)
{
	const struct spd_taskqueue *tps = obj;
	return spd_str_case_hash(tps->name);
}

static int tps_cmp_cb(void * obj, void * arg, int flags)
{
	struct spd_taskqueue *tps = obj, *rhs = arg;
	return !strcasecmp(tps->name, rhs->name) ? CMP_MATCH |CMP_STOP : 0;
}

static void tps_taskqueue_destroy(void * t)
{
	struct spd_taskqueue *tps = t;

	if(!tps) {
		spd_log(LOG_ERROR, "missing taskqueue\n");
		return;
	}

	spd_debug(1, "destroy taskqueue '%s' \n", tps->name);

	spd_mutex_lock(&tps->tskq_lock);
	tps->poll_thread_run = 0;
	spd_cond_signal(&tps->poll_cond);
	spd_mutex_unlock(&tps->tskq_lock);
	pthread_join(tps->poll_thread, NULL);
	tps->poll_thread = SPD_PTHREADT_NULL;
	spd_mutex_destroy(&tps->tskq_lock);
	spd_cond_destroy(&tps->poll_cond);
	if(tps->stats) {
		spd_safe_free(tps->stats);
	}
	free((char *)tps->name);
}


/* pop the front task and return it */
static struct tps_task *tps_taskqueue_pop(struct spd_taskqueue * tps)
{
	struct tps_task *task;
	if(!tps) {
		spd_log(LOG_ERROR, "missing taskqueue\n");
		return NULL;
	}

	spd_mutex_lock(&tps->tskq_lock);
	if((task = SPD_LIST_REMOVE_HEAD(&tps->tps_queue, list))) {
		tps->tps_queue_size--;
	}
	spd_mutex_unlock(&tps->tskq_lock);

	return task;
}

static int tps_taskqueue_depth(struct spd_taskqueue * tps)
{
	if(!tps) {
		spd_log(LOG_ERROR, "no taskqueue specified !\n");
		return -1;
	}

	return tps->tps_queue_size;
}

const char *spd_taskqueue_name(struct spd_taskqueue *tps)
{
	if(!tps) {
		spd_log(LOG_ERROR, "no taskqueue specified !\n");
		return NULL;
	}
	return tps->name;
}


/* provide a reference to a taskqueue , create the taskqueu if necessary , but donot create
   the taskqueue if we are told via spd_tps_option to return a reference only if it already exists
*/   
struct spd_taskqueue *spd_taskqueue_get(const char * name, enum spd_tps_options option)
{
	struct spd_taskqueue *p, tmp_tps = {
		.name = name,
	};

	if(spd_strlen_zero(name)) {
		spd_log(LOG_ERROR, "requesting a nameless taskqueue !!!\n");
		return NULL;
	}

	obj_lock(tps_singletons);
	p = obj_find(tps_singletons, &tmp_tps, OBJ_POINTER);
	if(p) {
		obj_unlock(tps_singletons);
		return p;
	}

	if(option & TPS_REF_IF_EXISTS) {
		/* calling function does not want a new taskprocessor to be created if it doesn't already exist */
		obj_unlock(tps_singletons);
		return NULL;
	}

	/* create a new taskprocessor */
	if(!(p = obj_alloc(sizeof(*p), tps_taskqueue_destroy))) {
		obj_unlock(tps_singletons);
		spd_log(LOG_WARNING, "failed to create taskqueue '%s'\n", name);
		return NULL;
	}
	spd_cond_init(&p->poll_cond, NULL);
	spd_mutex_init(&p->tskq_lock);

	if(!(p->stats = spd_calloc(1, sizeof(*p->stats)))) {
		obj_unlock(tps_singletons);
		spd_log(LOG_WARNING, "failed to create taskqueue '%s'\n", name);
		obj_ref(p, -1);
		return NULL;
	}

	if(!(p->name = spd_strdup(name))) {
		obj_unlock(tps_singletons);
		obj_ref(p, -1);
		return NULL;
	}

	p->poll_thread_run = -1;
	p->poll_thread = SPD_PTHREADT_NULL;

	if(spd_pthread_create(&p->poll_thread, NULL, tps_work_thread, p) < 0) {
		obj_unlock(tps_singletons);
		spd_log(LOG_ERROR, "failed to create work thread for taskqueue '%s' \n", p->name);
		obj_ref(p ,-1);
		return NULL;
	}

	if(!(obj_link(tps_singletons, p))) {
		obj_unlock(tps_singletons);
		spd_log(LOG_ERROR, "Failed to add tashqueue '%s' to container\n ", p->name);
		obj_ref(p, -1);
		return NULL;
	}

	obj_unlock(tps_singletons);

	return p;
}

void *spd_taskqueue_unreference(struct spd_taskqueue * tps)
{
	if(tps) {
		obj_lock(tps_singletons);
		obj_unlink(tps_singletons, tps);
		if(obj_ref(tps, -1) > 1) {
			obj_link(tps_singletons, tps);
		}
		obj_unlock(tps_singletons);
	}
	return NULL;
}

int spd_taskqueue_push(struct spd_taskqueue * tps, int(* task_exe)(void * data), void * data)
{
	struct tps_task *task;

	if(!tps || !task_exe) {
		spd_log(LOG_ERROR, " %s is missing !!\n",(tps)? "task calback" : "taskqueue");
		return -1;
	}

	if(!(task = tps_task_alloc(task_exe, data))) {
		spd_log(LOG_ERROR, " failed to allocate task! can not push to '%s'\n", tps->name);
		return -1;
	}

	spd_mutex_lock(&tps->tskq_lock);
	SPD_LIST_INSERT_TAIL(&tps->tps_queue, task,list);
	tps->tps_queue_size++;
	spd_cond_signal(&tps->poll_cond);
	spd_mutex_unlock(&tps->tskq_lock);

	return 0;
}