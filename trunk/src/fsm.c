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
#include "logger.h"
#include "fsm.h"
#include "utils.h"

struct spd_fsm *spd_fsm_create(spd_fsm_state_id cur, spd_fsm_state_id term)
{
	struct spd_fsm *tmp;

	if(!(tmp = spd_calloc(1, sizeof(*tmp)))) {
		return NULL;
	}
	
#ifdef DEBUG_FSM
	tmp->debug = 1;
#endif

	tmp->current =  cur;
	tmp->term = term;
	tmp->entry_list = SPD_LIST_HEAD_NOLOCK_INIT_VALUE;
	spd_mutex_init(&tmp->lock);

	return tmp;
}

int spd_fsm_destroy(struct spd_fsm *self)
{
	struct spd_fsm_entry *tmp;

	if(self) {
			
		spd_mutex_lock(&self->lock);
		while((tmp = SPD_LIST_REMOVE_HEAD(&self->entry_list, list))) {
			spd_safe_free(tmp);
		}
		spd_mutex_unlock(&self->lock);
		
		self->lock = spd_mutex_destroy(&self->lock);

		spd_safe_free(self);
		
		return 0;
	}

	return -1;
}

static struct spd_fsm_entry *spd_fsm_entry_create()
{
	struct spd_fsm_entry *entry;

	if(!(entry = spd_calloc(1, sizeof(*entry)))) {
		return NULL;
	}

	return entry;
}

int spd_fsm_cond_always(const void *cond1, const void *cond2)
{
	return 1;
}

int spd_fsm_exec_nothing(va_list * ap)
{
	return 0;
}

int spd_fsm_set(struct spd_fsm * self,...)
{
	va_list args;
	int guard;

	if(!self) {
		spd_log(LOG_ERROR, "invalid parameter\n");
		return -1;
	}

	va_start(args, self);
	while((guard = va_arg(args, int))) {
		struct spd_fsm_entry *entry;

		if((entry = spd_fsm_entry_create())){
			entry->from = va_arg(args, spd_fsm_state_id);
			entry->action = va_arg(args, spd_fsm_action_id);
			entry->cond = va_arg(args, spd_fsm_cond);
			entry->to = va_arg(args, spd_fsm_state_id);
			entry->exec = va_arg(args, spd_fsm_exec);
			entry->desc = va_arg(args, const char *);
			SPD_LIST_INSERT_TAIL(&self->entry_list, entry, list)
		}
	}
	va_end(args);
	
	return 0;
}

/*
* Execute an @a action. This action will probably change the current state of the FSM.
* @param self The FSM.
* @param action The id of the action to execute.
* @param cond_data1 The first opaque data to pass to the @a condition function.
* @param cond_data2 The first opaque data to pass to the @a condition function.
* @param ... Variable parameters to pass to the @a exec function.
* @retval Zero if succeed and non-zero error code otherwise.
*/
int spd_fsm_start(struct spd_fsm * self, spd_fsm_action_id action, const void * cond1, const void * cond2,...)
{
	struct spd_fsm_entry *tmp;
	va_list ap;
	int found = 0;
	int terminate = 0;
	int ret = 0;

	if(!self) {
		spd_log(LOG_ERROR, "invalid parameter\n");
		return -1;
	}

	if(spd_fsm_is_terminate(self)) {
		spd_log(LOG_WARNING, "Final state machine is in the final state.\n");
		return -2;
	}

	spd_mutex_lock(&self->lock);

	va_start(ap, cond2);
	SPD_LIST_TRAVERSE(&self->entry_list, tmp, list) {

		if((tmp->from != spd_fsm_action_any) && (tmp->from != self->current)) {
			continue;
		}

		if((tmp->action != spd_fsm_action_any) && (action != tmp->action)) {
			continue;
		}

		if(tmp->cond(cond1, cond2)) {
			if(self->debug) {
				spd_log(LOG_DEBUG, "state machine %s\n", tmp->desc);
			}

			if(tmp->to != spd_fsm_action_any) { /* Stay at the current state if dest. state is Any */
				self->current = tmp->to;
			}

			if(tmp->exec) {
				if((ret = tmp->exec(&ap))) {
					spd_log(LOG_ERROR, "State machine: Exec function failed. Moving to terminal state.");
				}
			} else {
				ret = 0;
			}
			terminate = (ret || (self->current == self->term));
			found = 1;
			break;
		}
	}
	va_end(ap);

	spd_mutex_unlock(&self->lock);

	if(terminate) {
		self->current = self->term;
		if(self->term_cb) {
			self->term_cb(self->data);
		}
	}

	if(!found){
		spd_log(LOG_DEBUG ,"State machine: Exec function failed. Moving to terminal state.\n");
	}
}

/*
* Sets the @a callback function to call when the FSM enter in the final state.
* @param self The FSM.
* @param callback The callback function to call.
* @param callbackdata Opaque data (user-data) to pass to the callback function.
* @retval Zero if succeed and non-zero error code otherwise.
*/
int spd_fsm_set_terminate_callback(struct spd_fsm * self, spd_fsm_onterminate_cb callback, const void * data)
{
	if(self) {
		self->term_cb = callback;
		self->data = data;
		return 0;
	}
	
	spd_log(LOG_ERROR, "invalid parameter\n");
	
	return -1;
}

int spd_fsm_is_terminate(struct spd_fsm * self)
{
	if(self) {
		return self->current == self->term;
	} else {
		spd_log(LOG_ERROR, "invalid parameter\n");
		return 1;
	}

	
}
