/*
 * Spider -- An open source xxx toolkit.
 *
 * Copyright (C) 2011 , Inc.
 *
 * lidp <774291943@qq.com>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#ifndef _SPIDER_OPTIONS_H
#define _SPIDER_OPTIONS_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* spider main options definenations */
enum spd_option_flags {
    /* Do not fork() */
	SPD_OPT_FLAG_NO_FORK = (1 << 0),
 	/* Console mode */
	SPD_OPT_FLAG_CONSOLE = (1 << 1),
	/* Remote Console */
	SPD_OPT_FLAG_REMOTE = (1 << 2),
    /* whether display timestamp on console */
    SPD_OPT_FLAG_TIMESTAMP = (1 << 3),
    /* whether allow core dump  */
	SPD_OPT_FLAG_CORE_DUMP = (1 << 4),
};

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif


#endif 
