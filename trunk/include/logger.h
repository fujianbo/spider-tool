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

#ifndef _SPIDER_LOGGER_H
#define _SPIDER_LOGGER_H

#include <stdarg.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif


#define VERBOSE_PREFIX_1 " "
#define VERBOSE_PREFIX_2 "  == "
#define VERBOSE_PREFIX_3 "    -- "
#define VERBOSE_PREFIX_4 "       > "

#define _LOG_    __FILE__, __LINE__ ,__PRETTY_FUNCTION__

#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#define __LOG_DEBUG    0
#define LOG_DEBUG      __LOG_DEBUG, _LOG_


#ifdef LOG_NOTICE
#undef LOG_NOTICE
#endif
#define __LOG_NOTICE   1
#define LOG_NOTICE __LOG_NOTICE, _LOG_


#ifdef LOG_WARNING
#undef LOG_WARNING
#endif
#define __LOG_WARNING  2
#define LOG_WARNING  __LOG_WARNING,  _LOG_

#ifdef LOG_ERROR
#undef LOG_ERROR
#endif
#define __LOG_ERROR    3
#define LOG_ERROR  __LOG_ERROR, _LOG_


#ifdef LOG_VERBOSE 
#undef LOG_VERBOSE
#endif 
#define __LOG_VERBOSE  4
#define LOG_VERBOSE __LOG_VERBOSE, _LOG_



/*! Used for sending a log message */
/*!
	\brief This is the standard logger function.  Probably the only way you will invoke it would be something like this:
	spd_log(LOG_WHATEVER, "Problem with the %s Captain.  We should get some more.  Will %d be enough?\n", "flux capacitor", 10);
	where WHATEVER is one of ERROR, DEBUG, EVENT, NOTICE, or WARNING depending
	on which log you wish to output to. These are implemented as macros, that
	will provide the function with the needed arguments.

 	\param level	Type of log event
	\param file	Will be provided by the LOG_* macro
	\param line	Will be provided by the LOG_* macro
	\param function	Will be provided by the LOG_* macro
	\param fmt	This is what is important.  The format is the same as your favorite breed of printf.  You know how that works, right? :-)
 */
void spd_log(int level, const char *file, int line, const char *function, const char *fmt, ...)
	__attribute__((format(printf, 5, 6)));

/* debug msg */
#define spd_debug(level, ...) do {      \
	if(option_debug >= (level))           \
		spd_log(LOG_DEBUG, __VA_ARGS__);  \
} while(0)

/*! Send a verbose message (based on verbose level)
 	\brief This works like ast_log, but prints verbose messages to the console depending on verbosity level set.
 	spd_verbose(VERBOSE_PREFIX_3 "Whatever %s is happening\n", "nothing");
 	This will print the message to the console if the verbose level is set to a level >= 3
 	Note the abscence of a comma after the VERBOSE_PREFIX_3.  This is important.
 	VERBOSE_PREFIX_1 through VERBOSE_PREFIX_3 are defined.
 */
void spd_verbose(const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

/* init logger engine*/
int init_logger(void);

/*! close file ptr after exit 
 */
void close_logger();

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* _SPIDER_LOGGER_H */

