/*! \file
 *
 * \brief Logging routines,Support for logging to console.
 *
 * \author lidp <lidp@cyclecentury.com>
 *
 * \extref  
 *
 *
 * - this is a wraper for  android log, user may see log in logcat.
 * - utility-related API functions, used by internal ccdt.
 * - 
 *
 * \ref none
 */

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


#define LOGLEVEL  5


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

/* log level define */
#define _LOG_    __FILE__, __LINE__ ,__PRETTY_FUNCTION__

#ifdef TSK_LOG_VERBOSE 
#undef TSK_LOG_VERBOSE
#endif 


#ifdef LOGGER_VERBOSE
#undef LOGGER_VERBOSE
#endif
#define LOGGER_VERBOSE   0
#define TSK_LOG_VERBOSE    LOGGER_VERBOSE, _LOG_

#ifdef LOGGER_DEBUG
#undef LOGGER_DEBUG
#endif

#ifdef TSK_LOG_DEBUG
#undef TSK_LOG_DEBUG
#endif
#define LOGGER_DEBUG    1
#define TSK_LOG_DEBUG      LOGGER_DEBUG, _LOG_


#ifdef LOGGER_NOTICE
#undef LOGGER_NOTICE
#endif
#ifdef TSK_LOG_NOTICE
#undef TSK_LOG_NOTICE
#endif
#define LOGGER_NOTICE   2
#define TSK_LOG_NOTICE LOGGER_NOTICE, _LOG_


#ifdef TSK_LOG_WARNING
#undef TSK_LOG_WARNING
#endif


#ifdef LOGGER_WARNING
#undef LOGGER_WARNING
#endif
#define LOGGER_WARNING 3
#define TSK_LOG_WARNING LOGGER_WARNING, _LOG_

#ifdef TSK_LOG_ERROR
#undef TSK_LOG_ERROR
#endif

#ifdef LOGGER_ERROR
#undef LOGGER_ERROR
#endif
#define LOGGER_ERROR   4
#define TSK_LOG_ERROR  LOGGER_ERROR, _LOG_

extern const  char *loglevels[];
extern int colors[];
extern  char dateformat[256];

/*  out put time in a format */
void print_time();

/* fileter special sequeces ,such as ESC */
void term_filter_escapes(char *line);

/* copy string , This is similar to strncpy, with two important differences:
    - the destination buffer will  always be null-terminated
    - the destination buffer is not filled with zeros past the copied string length
  */
void tsk_copy_string(char *dst, const char *src, size_t size);

/* format the input sring to specific color */
char *term_color(char *outbuf, const char *inbuf, int fgcolor, int bgcolor, int maxout);

/* out put log to console in diff level 
     the level is : LOG_DEBUG, LOG_NOTICE, LOG_VERBOSE, LOG_ERROR, LOG_WARNING
     an out put example  :[Jan  2 04:55:55] DEBUG[1604]: src/transactions/tsip_transac_layer.c:282 tsip_transac_layer
  */
void _tsk_log(int level, const char *file, int line, const char *function, const char *fmt, ...);

/*put a sring to console*/
void tsk_console_puts(char *str);


/*!
 * \brief Log a DEBUG message
 * \param level The minimum value of TSK_LOGLEVEL for this message
 *        to get logcat
 */

#define tsk_log(level, ...) do {       \
        if (level <= (LOGLEVEL) ) { \
		if(level == 0)  \
			_tsk_log(TSK_LOG_VERBOSE, __VA_ARGS__); \
		else if( level == 1) \
			_tsk_log(TSK_LOG_DEBUG, __VA_ARGS__); \
		else if(level == 2)  \
			_tsk_log(TSK_LOG_NOTICE, __VA_ARGS__); \
		else if(level == 3)  \
			_tsk_log(TSK_LOG_WARNING, __VA_ARGS__); \
		else if(level == 4)  \
			_tsk_log(TSK_LOG_ERROR, __VA_ARGS__); \
		else \
			_tsk_log(TSK_LOG_DEBUG, __VA_ARGS__); \
		} \
} while (0)

#endif
