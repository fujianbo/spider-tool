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


#ifndef SPIDER_CONST_H
#define SPIDER_CONST_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define ESC 0x1b
#define ATTR_RESET	0
#define ATTR_BRIGHT	1
#define ATTR_DIM	2
#define ATTR_UNDER	4
#define ATTR_BLINK	5
#define ATTR_REVER	7
#define ATTR_HIDDEN	8


/* termanal color define */

#define COLOR_BLACK 	30
#define COLOR_GRAY  	(30 | 128)
#define COLOR_RED	31
#define COLOR_BRRED	(31 | 128)
#define COLOR_GREEN	32
#define COLOR_BRGREEN	(32 | 128)
#define COLOR_BROWN	33
#define COLOR_YELLOW	(33 | 128)
#define COLOR_BLUE	34
#define COLOR_BRBLUE	(34 | 128)
#define COLOR_MAGENTA	35
#define COLOR_BRMAGENTA (35 | 128)
#define COLOR_CYAN      36
#define COLOR_BRCYAN    (36 | 128)
#define COLOR_WHITE     37
#define COLOR_BRWHITE   (37 | 128)

/* some default dir definition */

#define spd_config_SPD_LOG_DIR  "/var/log/spider"
#define spd_config_SPD_LOGGER_FILE  "/etc/spider/logger.conf"
#define spd_config_maincfg  "/etc/spider/spider.conf"
#define spd_config_SPD_CONFIG_DIR  "/etc/spider"


int option_debug ;
int option_verbose;

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif

