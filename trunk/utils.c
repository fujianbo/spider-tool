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

#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>


#include "utils.h"
#include "lock.h"
#include "linkedlist.h"
#include "const.h"
#include "threadprivdata.h"
#include "time.h"

long int spd_random(void)
{
    long int res;

    #ifdef linux
        res = random();
    #else
    spd_mutex_lock(&spd_random_lock);
    res = random();
    spd_mutex_unlock(&spd_random_lock);
    #endif

    return res;
}

int spd_true(const char *s)
{
    if(!s)
        return 0;

    if(!strcasecmp(s, "yes") || !strcasecmp(s, "true") ||
        !strcasecmp(s, "y") || !strcasecmp(s, "t") ||
        !strcasecmp(s, "1") || !strcasecmp(s, "on"))
        return -1;
    return 0;
}

int spd_false(const char *s)
{
    if(!s)
        return 0;

    if (!strcasecmp(s, "no") ||
	    !strcasecmp(s, "false") ||
	    !strcasecmp(s, "n") ||
	    !strcasecmp(s, "f") ||
	    !strcasecmp(s, "0") ||
	    !strcasecmp(s, "off"))
		return -1;

    return 0;
}

int spd_mkdir(const char *path, int mode)
{
    char *p = NULL;
    int len = strlen(path), count = 0, x, piececount = 0;
    char *tmp = spd_strdup(path);
    char **pieces;
    char *fullpath = alloca(len + 1);
    int res = 0;

    for(p = tmp; *p; p++) {
        if(*p == '/')
            count++;
    }
    pieces = alloca(count * sizeof(*pieces));
    for(p = tmp; *p; p++) {
        if(*p == '/') {
            *p = '\0';
            pieces[piececount++] = p + 1;
        }
    }

    *fullpath = '\0';
    for(x = 0; x < piececount; x++) {
        strcat(fullpath, "/");
        strcat(fullpath, pieces[x]);
        res = mkdir(fullpath, mode);
        if(res && errno != EEXIST)
            return errno;
    }
    return 0;
}


#define ONE_MILLION	1000000
/*
 * put timeval in a valid range. usec is 0..999999
 * negative values are not allowed and truncated.
 */
static struct timeval tvfix(struct timeval a)
{
	if (a.tv_usec >= ONE_MILLION) {
		spd_log(LOG_WARNING, "warning too large timestamp %ld.%ld\n",
			(long)a.tv_sec, (long int) a.tv_usec);
		a.tv_sec += a.tv_usec / ONE_MILLION;
		a.tv_usec %= ONE_MILLION;
	} else if (a.tv_usec < 0) {
		spd_log(LOG_WARNING, "warning negative timestamp %ld.%ld\n",
			(long)a.tv_sec, (long int) a.tv_usec);
		a.tv_usec = 0;
	}
	return a;
}

struct timeval spd_tvadd(struct timeval a, struct timeval b)
{
	/* consistency checks to guarantee usec in 0..999999 */
	a = tvfix(a);
	b = tvfix(b);
	a.tv_sec += b.tv_sec;
	a.tv_usec += b.tv_usec;
	if (a.tv_usec >= ONE_MILLION) {
		a.tv_sec++;
		a.tv_usec -= ONE_MILLION;
	}
	return a;
}

struct timeval spd_tvsub(struct timeval a, struct timeval b)
{
	/* consistency checks to guarantee usec in 0..999999 */
	a = tvfix(a);
	b = tvfix(b);
	a.tv_sec -= b.tv_sec;
	a.tv_usec -= b.tv_usec;
	if (a.tv_usec < 0) {
		a.tv_sec-- ;
		a.tv_usec += ONE_MILLION;
	}
	return a;
}
#undef ONE_MILLION

/* filter escape sequences */
void term_filter_escapes(char *line)
{
	int i;
	int len = strlen(line);

	for (i = 0; i < len; i++) {
		if (line[i] != ESC)
			continue;
		if ((i < (len - 2)) &&
		    (line[i + 1] == 0x5B)) {
			switch (line[i + 2]) {
		 	case 0x30:
			case 0x31:
			case 0x33:
				continue;
			}
		}
		/* replace ESC with a space */
		line[i] = ' ';
	}
}


char *term_color(char *outbuf, const char *inbuf, int fgcolor, int bgcolor, int maxout)
{
	int attr=0;
	char tmp[40];
	
	if (!fgcolor && !bgcolor) {
		spd_copy_string(outbuf, inbuf, maxout);
		return outbuf;
	}
	if ((fgcolor & 128) && (bgcolor & 128)) {
		/* Can't both be highlighted */
		spd_copy_string(outbuf, inbuf, maxout);
		return outbuf;
	}
	if (!bgcolor)
		bgcolor = COLOR_BLACK;

	if (bgcolor) {
		bgcolor &= ~128;
		bgcolor += 10;
	}
	if (fgcolor & 128) {
		attr = ATTR_BRIGHT;
		fgcolor &= ~128;
	}
	if (fgcolor && bgcolor) {
		snprintf(tmp, sizeof(tmp), "%d;%d", fgcolor, bgcolor);
	} else if (bgcolor) {
		snprintf(tmp, sizeof(tmp), "%d", bgcolor);
	} else if (fgcolor) {
		snprintf(tmp, sizeof(tmp), "%d", fgcolor);
	}
	if (attr) {
		snprintf(outbuf, maxout, "%c[%d;%sm%s%c[0;%d;%dm", ESC, attr, tmp, inbuf, ESC, 
    		COLOR_WHITE, COLOR_BLACK + 10);
	} else {
		snprintf(outbuf, maxout, "%c[%sm%s%c[0;%d;%dm", ESC, tmp, inbuf, ESC, COLOR_WHITE, 
    		COLOR_BLACK + 10);
	}
	return outbuf;
}
