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
 
#ifndef _SPIDER_H
#define _SPIDER_H


#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/resource.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>



//int option_debug;
//int option_verbose;

/*!
 * \brief Register a function to be executed before spider exits.
 * \param func The callback function to use.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
int spd_register_atexit(void (*func)(void));

/*!
 * \brief Unregister a function registered with spd_register_atexit().
 * \param func The callback function to unregister.
 */
void spd_unregister_atexit(void (*func)(void));


typedef struct {
	char *mod_dir;
	char *conf_dir;
	char *run_dir;
	char *log_dir;
}spider_global_dir;

//extern spider_global_dir SPIDER_GLOGAL_dirs = {0};

#endif
