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
 
/*
   \brief Finite-state machine (FSM) implementation.
    see http://en.wikipedia.org/wiki/Finite-state_machine.
          http://en.wikipedia.org/wiki/Virtual_finite_state_machine
 */

#ifndef _SPIDER_FSM_H
#define _SPIDER_FSM_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "linkedlist.h"
#include "lock.h"

enum spd_fsm_state_e {
	spd_fsm_state_any = -0xFFFF,
	spd_fsm_state_default = -0xFFF0,
	spd_fsm_state_none = -0xFF00,
	spd_fsm_state_final = -0xF000,
};

enum spd_fsm_action_e {
	spd_fsm_action_any = -0xFFFF,
};

typedef int spd_fsm_state_id;
typedef int spd_fsm_action_id;
typedef int (*spd_fsm_cond)(const void *, const void *);
typedef int (*spd_fsm_exec)(va_list *app);
typedef int (*spd_fsm_onterminate_cb)(const void *);

#define SPD_FSM_ADD(from, action , cond, to, exec, desc) \
	1,\
	(spd_fsm_state_id)from, \
	(spd_fsm_action_id)action, \
	(spd_fsm_cond)cond, \
	(spd_fsm_state_id)to, \
	(spd_fsm_exec)exec, \
	(const char *)desc

#define SPD_FSM_ADD_ALWAYS(from, action, to, exec, desc) \
	SPD_FSM_ADD(from, action, spd_fsm_cond_always, to, exec, desc)

#define SPD_FSM_ADD_NOTHING(from, action, cond, desc) SPD_FSM_ADD(from, action, cond, from, spd_fsm_exec_nothing, desc)

#define SPD_FSM_ADD_ALWAYS_NOTHING(from, desc)	SPD_FSM_ADD(from, spd_fsm_action_any, spd_fsm_cond_always, from, spd_fsm_exec_nothing, desc)

#define SPD_FSM_ADD_DEFAULT()

#define SPD_FSM_ADD_NULL()\
	NULL
	
struct spd_fsm_entry {
	spd_fsm_state_id from;
	spd_fsm_action_id action;
	spd_fsm_cond cond;
	spd_fsm_state_id to;
	spd_fsm_exec exec;
	const char *desc;
	SPD_LIST_ENTRY(spd_fsm_entry)list;
};

struct spd_fsm {
	unsigned int debug:1;
	spd_fsm_state_id current;
	spd_fsm_state_id term;
	SPD_LIST_HEAD_NOLOCK(, spd_fsm_entry)entry_list;
	spd_fsm_onterminate_cb   term_cb;
	const void *data;
	spd_mutex_t lock;
};

struct spd_fsm *spd_fsm_create(spd_fsm_state_id cur, spd_fsm_state_id term);
int spd_fsm_destroy(struct spd_fsm *fsm);
int spd_fsm_exec_nothing(va_list *ap);
int spd_fsm_cond_always(const void *, const void *);
int spd_fsm_set(struct spd_fsm *self, ...);
int spd_fsm_set_terminate_callback(struct spd_fsm *self, spd_fsm_onterminate_cb callback, const void *data);
int spd_fsm_start(struct spd_fsm *self, spd_fsm_action_id action, const void *cond1, const void *cond2, ...);
int spd_fsm_is_terminate(struct spd_fsm *self);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
