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

#include "utils.h"
#include "lock.h"
#include "linkedlist.h"

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