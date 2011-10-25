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

#include "linkedlist.h"
#include "cli.h"
#include "lock.h"
#include "threadprivdata.h"
#include "str.h"
 
SPD_THREADPRIVDATA(spd_cli_buf);

static SPD_RWLIST_HEAD_STATIC(spd_cli_entrys, spd_cli_entry);

#define SPD_CLI_BUF_LEN    256

/*!
 * Some regexp characters in cli arguments are reserved and used as separators.
 */
static const char cli_rsvd[] = "[]{}|*%";
int spd_cli(int fd, const char *fmt, ...)
{
 	int res;
	struct spd_str *buf;
	va_list ap;

	if(!(buf = spd_str_thread_get(&spd_cli_buf, SPD_CLI_BUF_LEN))){
		return;
	}

	va_start(ap, fmt);
	res = spd_str_set_va(&buf, 0, fmt, ap);
	va_end(ap);

	if(res != SPD_STR_BUILD_FAILED) {
		spd_timeout_write(fd, spd_str_buffer(buf), spd_str_len(buf), 100) /* 100 ms default */
	}
}

static struct spd_cli_entry *cli_next(struct spd_cli_entry *e)
{
	if(e) {
		SPD_LIST_NEXT(e,list);
	} else {
		SPD_LIST_FIRST(&spd_cli_entrys);
	}
}

static char *__spd_cli_generator(const char *text, const char *word, int state, int lock);

/*
 * helper function to generate CLI matches from a fixed set of values.
 * A NULL word is acceptable.
 */
char *spd_cli_complete(const char * word, const char * const choices[ ], int state)
{
	int i, which = 0, len;
	len = spd_str_len(word) ? 0 : strlen(word);
	for(i = 0; choices[i]; i++) {
		if((!len || !strncasecmp(word, choices[i], len)) && ++ which > state)
			return spd_strdup(choices[i]);
	}

	return NULL;
}

static int spd_cli_set_full_cmd(struct spd_cli_entry *e)
{
	int i;
	char buf[80];
	spd_str_join(buf, sizeof(buf), e->cmda);

	e->_full_cmd = spd_strdup(buf);
	if(!e->_full_cmd) {
		return -1;
	}
	e->cmdlen = strcspn(e->_full_cmd, cli_rsvd);
	for(i = 0; e->cmda[i]; i++)
		;
	e->args = i;
	
	return 0;
}

