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

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#include "scheduler.h"
#include "utils.h"
#include "linkedlist.h"
#include "logger.h"
#include "lock.h"
#include "times.h"


#define DEBUG_M(a) {\
	(a);  \
}

#ifdef DEBUG_SCHEDULER \
	#define DEBUG(a) do {
	if(option_debug) \
		DEBUG_M(a)  \
} while(0)
#else 
#define DEBUG(a)

struct scheduler {
	int flag;              /*!< Use return value from callback to reschedule */
	int reschedule;        /*!< When to reschedule */
	int id;                /*!< ID number of event */
	struct timeval when;   /*!< Absolute time event should take place */
	spd_scheduler_cb callback;
	void *data;
	SPD_LIST_ENTRY(scheduler)list;
};

struct scheduler_context {
	spd_mutex_t lock;
	unsigned int processedcnt;                         /*!< Number of events processed */
	unsigned int schedsnt;		                       /*!< Number of outstanding schedule events */
	SPD_LIST_HEAD_NOLOCK(, scheduler)schedulerq;       /*!< Schedule entry and main queue */
#ifdef SPD_SCHED_MA_CACHE
	SPD_LIST_HEAD_NOLOCK(, scheduler)schedulerc;
	unsigned int schedccnt;
#endif
};

struct scheduler_context *spd_sched_context_create(void)
{
	struct scheduler_context *sc;

	if(!(sc = spd_calloc(1, sizeof(*sc)))) {
		return NULL;
	}

	spd_mutex_init(&sc->lock);
	sc->processedcnt = 1;

	return sc;
}

void spd_sche_context_destroy(struct scheduler_context *sc)
{
	struct scheduler *s;

	spd_mutex_lock(&sc->lock);
	
#ifdef SPD_SCHED_MA_CACHE
	while((s = SPD_LIST_REMOVE_HEAD(&sc->schedulerc, list)))
		spd_safe_free(s);
#endif

	while((s = SPD_LIST_REMOVE_HEAD(&sc->schedulerq, list)))
		spd_safe_free(s);

	spd_mutex_unlock(&sc->lock);

	spd_mutex_destroy(&s->lock)

	spd_safe_free(sc);
}

static struct scheduler *sched_alloc(struct scheduler_context *con)
{
	struct scheduler *tmp;

#ifdef SPD_SCHED_MA_CACHE
	if((tmp = SPD_LIST_REMOVE_CURRENT(&con->schedulerc, list)))
			con->schedccnt--;
	else
#endif 

	if(!(tmp = spd_calloc(1, sizeof(*tmp))))
		return NULL;

	return tmp;
}

static void scheduler_release(struct scheduler_context *con, struct scheduler *sc)
{

	#ifdef SPD_SCHED_MA_CACHE
		if(con->schedccnt < SPD_SCHED_MA_CACHE) {
			SPD_LIST_INSERT_HEAD(&con->schedulerc, sc, list);
			con->schedccnt++;
		} 
		else
	#endif

		spd_safe_free(sc);
}

/*! \brief
 * Return the number of milliseconds 
 * until the next scheduled event
 */
int spd_sched_wait(struct scheduler_context * c)
{
	int ms;
	DEBUG(spd_log(LOG_DEBUG, "ast_sched_wait()\n"));

	spd_mutex_lock(&c->lock);
	if(SPD_LIST_EMPTY(&c->schedulerq)){
		ms = -1;
	} else {
		ms = spd_tvdiff_ms(SPD_LIST_FIRST(&c->schedulerq)->when, spd_tvnow());
		if(ms < 0)
			ms = 0;
	}
	spd_mutex_lock(&c->lock);

	return ms;
}

/*! \brief
 * Take a sched structure and put it in the
 * queue, such that the soonest event is
 * first in the list.
 */
static void add_scheduler(struct scheduler_context *c, struct scheduler *s)
{
	struct scheduler cur = NULL;

	SPD_LIST_TRAVERSE_SAFE_BEGIN(&c->schedulerq, cur, list) {
		if(spd_tvcmp(s->when, cur->when)  == -1){
			SPD_LIST_INSERT_BEFORE_CURRENT(&c->schedulerq, s);
			break;
		}
	}
	SPD_LIST_TRAVERSE_SAFE_END

	if(!cur) {
		SPD_LIST_INSERT_TAIL(&c->schedulerq, s, list);
	}

	c->schedsnt;
}

/*! \brief
 * given the last event *tv and the offset in milliseconds 'when',
 * computes the next value,
 */
static int sched_settime(struct timeval *tv, int when)
{
	struct timeval now = spd_tvnow();
	if(spd_tvzero(*tv))
		*tv = now;
	*tv = spd_tvadd(*tv, spd_samp2tv(when, 1000));
	if(spd_tvcmp(*tv, now) < 0) {
		spd_log(LOG_DEBUG, "Request to schedule in the past ?\n");
		*tv = now;
	}
	return 0;
}

/*! \brief
 * Schedule callback(data) to happen when ms into the future
 */
int spd_sched_add_flag(struct scheduler_context * con, int when, spd_scheduler_cb callback, int flag)
{
	struct scheduler *tmp;
	int res = -1;

	DEBUG(spd_log(LOG_DEBUG, "spd_sched _add() \n"));
	if(!when) {
		spd_log(LOG_DEBUG, " scheduled event in 0 ms ? \n");
		return -1;
	}

	spd_mutex_lock(&con->lock);
	if((tmp = sched_alloc(con))) {
		tmp->id = con->processedcnt++;
		tmp->callback = callback;
		tmp->data = data;
		tmp->reschedule = when;
		tmp->flag = flag;
		tmp->when = spd_tv(0,0);
		if(sched_settime(&tmp->when, when)) {
			scheduler_release(con, tmp);
		} else {
			add_scheduler(con, tmp);
			res = tmp->id;
		}
	}

#ifdef DUMP_SCHEDULER
	/* Dump contents of the context while we have the lock so nothing gets screwed up by accident. */
	if (option_debug)
		spd_sched_dump(con);
#endif

	spd_mutex_unlock(&con->lock);
	return res;
}

int spd_sched_add(struct scheduler_context * con, int when, spd_scheduler_cb callback, void * data)
{
	return spd_sched_add_flag(con, when, callback,data, 0);
}

/*! \brief
 * Delete the schedule entry with number
 * "id".  It's nearly impossible that there
 * would be two or more in the list with that
 * id.
 */
int spd_sched_del(struct scheduler_context * c, int id)
{
	struct scheduler *s;

	spd_mutex_lock(&c->lock);
	SPD_LIST_TRAVERSE_SAFE_BEGIN(&c->schedulerq, s, list) {
		if(s->id == id) {
			SPD_LIST_REMOVE_CURRENT(&c->schedulerq, list);
			c->schedsnt--;
			scheduler_release(c, s);
			break;
		}
	}
	SPD_LIST_TRAVERSE_SAFE_END
	spd_mutex_unlock(&c->lock);

	if(!s) {
		spd_log(LOG_WARNING, "ask to delete null schedule\n");
		return -1;
	}

	return 0;
}

void spd_sched_dump(const struct scheduler_context *con)
{
	struct scheduler *q;
	struct timeval tv= spd_tvnow();
	#ifdef SPD_SCHED_MA_CACHE
	spd_log(LOG_DEBUG, " Schedule Dump (%d in Q, %d Total, %d Cache)\n", con->schedsnt, con->processedcnt- 1, con->schedccnt);
#else
	spd_log(LOG_DEBUG, " Schedule Dump (%d in Q, %d Total)\n", con->schedsnt, con->processedcnt- 1);
#endif

	spd_log(LOG_DEBUG, "=============================================================\n");
	spd_log(LOG_DEBUG, "|ID    Callback          Data              Time  (sec:ms)   |\n");
	spd_log(LOG_DEBUG, "+-----+-----------------+-----------------+-----------------+\n");
	SPD_LIST_TRAVERSE(&con->schedulerq, q, list) {
		struct timeval delta = spd_tvsub(q->when, tv);

		spd_log(LOG_DEBUG, "|%.4d | %-15p | %-15p | %.6ld : %.6ld |\n", 
			q->id,
			q->callback,
			q->data,
			delta.tv_sec,
			(long int)delta.tv_usec);
	}
	spd_log(LOG_DEBUG, "=============================================================\n");
	
}

/*! \brief
 * Launch all events which need to be run at this time.
 */
int spd_sched_runall(struct scheduler_context * c)
{
	struct scheduler *cur;

	struct timeval tv;
	int numevents;
	int res;

	spd_mutex_lock(&c->lock);

	for(numevents = 0; !SPD_LIST_EMPTY(&c->schedulerq); numevents++) {
		/* schedule all events which are going to expire within 1ms.
		 * We only care about millisecond accuracy anyway, so this will
		 * help us get more than one event at one time if they are very
		 * close together.
		 */
		 tv = spd_tvadd(spd_tvnow(), spd_tv(0, 1000));
		if(spd_tvcmp(SPD_LIST_FIRST(&c->schedulerq)->when, tv) != -1)
			break;

		cur = SPD_LIST_REMOVE_CURRENT(&c->schedulerq, list);
		c->schedsnt--;

		/*
		 * At this point, the schedule queue is still intact.  We
		 * have removed the first event and the rest is still there,
		 * so it's permissible for the callback to add new events, but
		 * trying to delete itself won't work because it isn't in
		 * the schedule queue.  If that's what it wants to do, it 
		 * should return 0.
		 */

		spd_mutex_unlock(&c->lock);
		res = cur->callback(cur->data);
		spd_mutex_lock(&c->lock);

		if(res) {
			/*
			 * If they return non-zero, we should schedule them to be
			 * run again.
			 */
			 if(sched_settime(&cur->when, cur->flag ? res : cur->reschedule)) {
				scheduler_release(c,cur);
			 } else 
				 add_scheduler(c, cur);
		} else {
			scheduler_release(c, cur);
		}
		spd_mutex_unlock(&c->lock);
	}

	return numevents;
}

long spd_sched_when(struct scheduler_context * con, int id)
{
	struct scheduler *s;
	long secs = -1;
	DEBUG(spd_log(LOG_DEBUG, "spd_sched_when()\n"));

	spd_mutex_lock(&con->lock);
	SPD_LIST_TRAVERSE(&con->schedulerq, s, list) {
		if (s->id == id)
			break;
	}
	if (s) {
		struct timeval now = spd_tvnow();
		secs = s->when.tv_sec - now.tv_sec;
	}
	spd_mutex_unlock(&con->lock);
	
	return secs;
}


