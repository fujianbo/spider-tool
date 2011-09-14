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
#define __LOG_WARNING 2
#define LOG_WARNING __LOG_WARNING, _LOG_

#ifdef LOG_ERROR
#undef LOG_ERROR
#endif
#define __LOG_ERROR   3
#define LOG_ERROR  __LOG_ERROR, _LOG_


#ifdef LOG_VERBOSE 
#undef LOG_VERBOSE
#endif 
#define __LOG_VERBOSE  4
#define LOG_VERBOSE __LOG_VERBOSE, _LOG_


#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* _SPIDER_LOGGER_H */

