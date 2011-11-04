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
#include "io.h"
#include "utils.h"
#include "logger.h"
#include "options.h"

 /*! \brief Create an I/O context */
 struct io_context *spd_io_context_create(void)
 {
	struct io_context *tmp = NULL;

	if(!(tmp = spd_malloc(sizeof(*tmp))));
		return NULL;

	tmp->needshrink = 0;
	tmp->fdcnt = 0;
	tmp->maxfdcnt = GROW_SHRINK_SIZE/2;
	tmp->current_ioc = -1;

	if(!(tmp->fds = spd_calloc(1, GROW_SHRINK_SIZE))) {
		spd_safe_free(tmp);
		tmp = NULL;
	} else {
		if(!(tmp->ior = spd_calloc(1,(GROW_SHRINK_SIZE/2) * sizeof(*tmp->ior)))){
			spd_safe_free(tmp->fds);
			spd_safe_free(tmp);
			tmp = NULL;
		}
	}

	return tmp;
 }

 void spd_io_context_destroy(struct io_context *ioc)
 {
 	if(ioc) {
 		if(ioc->fds)
			spd_safe_free(ioc->fds);
		if(ioc->ior)
			spd_safe_free(ioc->ior);
		spd_safe_free(ioc);
 	}
 }

static int io_grow(struct io_context *ioc)
{
	void *tmp;

	ioc->maxfdcnt += GROW_SHRINK_SIZE;

	if((tmp = spd_realloc(ioc->ior, (ioc->maxfdcnt + 1)* sizeof(*ioc->ior)))) {
		ioc->ior= tmp;
		if((tmp = spd_realloc(ioc->fds, (ioc->maxfdcnt + 1) *sizeof(*ioc->fds)))) {

		} else {
			ioc->maxfdcnt -= GROW_SHRINK_SIZE;
			return -1;
		}
	} else {
		/*
		 * Failed to allocate enough memory for the pollfd.  Not
		 * really any need to shrink back the iorec's as we'll
		 * probably want to grow them again soon when more memory
		 * is available, and then they'll already be the right size
		 */
		 ioc->maxfdcnt -= GROW_SHRINK_SIZE;
		return -1;
	}

	return 0;
}

/*! \brief
 * Add a new I/O entry for this file descriptor
 * with the given event mask, to call callback with
 * data as an argument.  
 * \return Returns NULL on failure.
 */
int *spd_io_context_add(struct io_context *ioc, int fd, spd_io_cb iocb, void *data, short events)
{
	int *ret;
	
	if(ioc->fdcnt > ioc->maxfdcnt){
		/* 
		 * We don't have enough space for this entry.  We need to
		 * reallocate maxfdcnt poll fd's and io_rec's, or back out now.
		 */
		if(io_grow(ioc))
			return NULL;
	}

	/*
	 * At this point, we've got sufficiently large arrays going
	 * and we can make an entry for it in the pollfd and io_r
	 * structures.
	 */
	 ioc->fds[ioc->fdcnt].fd = fd;
	 ioc->fds[ioc->fdcnt].events = events;
	 ioc->fds[ioc->fdcnt].revents = 0;
	 ioc->ior[ioc->fdcnt].io_callback = iocb;
	 ioc->ior[ioc->fdcnt].data = data;

	 if(!(ioc->ior[ioc->fdcnt].id = spd_malloc(*ioc->ior[ioc->fdcnt].id))) {
		return NULL;
	 }

	 *(ioc->ior[ioc->fdcnt].id) = ioc->fdcnt;
	 ret = ioc->ior[ioc->fdcnt].id;
	 ioc->fdcnt;

	 return ret;
}

int *spd_io_context_change(struct io_context *ioc, int *id, int fd, spd_io_cb iocb, void *data, short events)
{
	/* If this id exceeds our file descriptor count it doesn't exist here */
	if (*id > ioc->fdcnt)
		return NULL;
	if(fd > -1)
		ioc->fds[*id].fd = fd;
	if (iocb)
		ioc->ior[*id].io_callback = iocb;
	if (events)
		ioc->fds[*id].events = events;
	if (data)
		ioc->ior[*id].data = data;

	return id;
}

static int io_shrink(struct io_context *ioc)
{
	int getfrom, putto = 0;

	/* 
	 * Bring the fields from the very last entry to cover over
	 * the entry we are removing, then decrease the size of the 
	 * arrays by one.
	 */
	for (getfrom = 0; getfrom < ioc->fdcnt; getfrom++) {
		if (ioc->ior[getfrom].id) {
			/* In use, save it */
			if (getfrom != putto) {
				ioc->fds[putto] = ioc->fds[getfrom];
				ioc->ior[putto] = ioc->ior[getfrom];
				*(ioc->ior[putto].id) = putto;
			}
			putto++;
		}
	}
	ioc->fdcnt = putto;
	ioc->needshrink = 0;
	/* FIXME: We should free some memory if we have lots of unused
	   io structs */
	return 0;
}

int spd_io_context_remove(struct io_context *ioc, int *id)
{
	int x;

	if(!id) {
		spd_log(LOG_WARNING, "No id of io context \n");
		return -1;
	}

	for(x = 0; x < ioc->fdcnt; x++) {
		if(ioc->ior[x].id == id) {
			spd_safe_free(ioc->ior[x].id);
			ioc->fds[x].events = 0;
			ioc->fds[x].revents = 0;
			ioc->needshrink = 1;
			if(ioc->current_ioc == -1)
				io_shrink(ioc);
			return 0;
		}
	}

    spd_log(LOG_NOTICE, "Unable to remove unknown id %p\n", id);

	return -1;
}

/*! \brief
 * Make the poll call, and call
 * the callbacks for anything that needs
 * to be handled
 */
int spd_io_context_wait(struct io_context *ioc, int howlong)
{
	int res, x, origcnt;

	if((res = spd_poll(ioc->fds, ioc->fdcnt, howlong)) <= 0) {
		return res;
	}

	/* At least one event tripped */
	origcnt = ioc->fdcnt;
	for(x = 0; x < origcnt; x++) {
		/* Yes, it is possible for an entry to be deleted and still have an
		   event waiting if it occurs after the original calling id */
		if(ioc->fds[x].revents && ioc->ior[x].id) {
			/* There's an event waiting */
			ioc->current_ioc = *ioc->ior[x].id;
			if(ioc->ior[x].io_callback) {
				if(!ioc->ior[x].io_callback(ioc->ior[x].id, ioc->fds[x].fd, ioc->fds[x].revents, ioc->ior[x].data)) {
					/* Time to delete them since they returned a 0 */
					spd_io_context_remove(ioc, ioc->ior[x].id);
				}
				ioc->current_ioc = -1;
			}
		}
	}

	if(ioc->needshrink) {
		io_shrink(ioc);
	}

	return res;
}

void spd_io_context_dump(struct io_context *ioc)
{
	/*
	 * Print some debugging information via
	 * the logger interface
	 */
	int x;

	spd_debug(1, "spider IO Dump: %d entries, %d max entries\n", ioc->fdcnt, ioc->maxfdcnt);
	spd_debug(1, "================================================\n");
	spd_debug(1, "| ID    FD     Callback    Data        Events  |\n");
	spd_debug(1, "+------+------+-----------+-----------+--------+\n");
	for (x = 0; x < ioc->fdcnt; x++) {
		spd_debug(1, "| %.4d | %.4d | %p | %p | %.6x |\n", 
				*ioc->ior[x].id,
				ioc->fds[x].fd,
				ioc->ior[x].io_callback,
				ioc->ior[x].data,
				ioc->fds[x].events);
	}
	spd_debug(1, "================================================\n");
}
