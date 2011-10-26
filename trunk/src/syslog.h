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
 
 /*! \file
 * \brief System log interface support 
 */
 
#ifndef _SPIDER_SYSLOG_H
#define _SPIDER_SYSLOG_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif


/*!
 * \brief Maps a syslog facility name from a string to a syslog facility
 *        constant.
 *
 * \param facility Facility name to map (i.e. "daemon")
 *
 * \retval syslog facility constant (i.e. LOG_DAEMON) if found
 * \retval -1 if facility is not found
 */
int spd_syslog_facility(const char *facility);

/*!
 * \brief Maps a syslog facility constant to a string.
 *
 * \param facility syslog facility constant to map (i.e. LOG_DAEMON)
 *
 * \retval facility name (i.e. "daemon") if found
 * \retval NULL if facility is not found
 */
const char *spd_syslog_facility_name(int facility);

/*!
 * \brief Maps a syslog priority name from a string to a syslog priority
 *        constant.
 *
 * \param priority Priority name to map (i.e. "notice")
 *
 * \retval syslog priority constant (i.e. LOG_NOTICE) if found
 * \retval -1 if priority is not found
 */
int spd_syslog_priority(const char *priority);

/*!
 * \brief Maps a syslog priority constant to a string.
 *
 * \param priority syslog priority constant to map (i.e. LOG_NOTICE)
 *
 * \retval priority name (i.e. "notice") if found
 * \retval NULL if priority is not found
 */
const char *spd_syslog_priority_name(int priority);

/*!
 * \brief Maps an  log level (i.e. LOG_ERROR) to a syslog priority
 *        constant.
 *
 * \param level Asterisk log level constant (i.e. LOG_ERROR)
 *
 * \retval syslog priority constant (i.e. LOG_ERR) if found
 * \retval -1 if priority is not found
 */
int spd_syslog_loglevel_to_priority(int loglevel);


#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif

