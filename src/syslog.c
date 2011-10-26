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

#include <syslog.h>

#include "syslog.h"
#include "utils.h"


struct spd_syslog_map{
	const char *name;
	int value;
};
static const struct spd_syslog_map syslog_map [] = {
	{ "user",     LOG_USER     },
	{ "local0",   LOG_LOCAL0   },
	{ "local1",   LOG_LOCAL1   },
	{ "local2",   LOG_LOCAL2   },
	{ "local3",   LOG_LOCAL3   },
	{ "local4",   LOG_LOCAL4   },
	{ "local5",   LOG_LOCAL5   },
	{ "local6",   LOG_LOCAL6   },
	{ "local7",   LOG_LOCAL7   },
	{ "kern",     LOG_KERN     },
	{ "mail",     LOG_MAIL     },
	{ "daemon",   LOG_DAEMON   },
	{ "ftp",      LOG_FTP      },
	{ "cron",     LOG_CRON     },
};

int spd_syslog_facility(const char * facility)
{
	int i;

	for(i = 0; i < ARRAY_LEN(syslog_map);  i++) {
		if(!strcasecmp(syslog_map[i].name, facility)) {
			return syslog_map[i].value;
		}
	}
	return -1;
}

const char *spd_syslog_facility_name(int facility)
{
	int i;
	for(i = 0; i < ARRAY_LEN(syslog_map); i++) {
		if(syslog_map[i].value == facility) {
			return syslog_map[i].name;
		}
	}

	return NULL;
}

struct spd_priority_map {
	const char *name;
	int value;
};

static const struct spd_priority_map priority_map [] = {
	{ "alert",   LOG_ALERT   },
	{ "crit",    LOG_CRIT    },
	{ "debug",   LOG_DEBUG   },
	{ "emerg",   LOG_EMERG   },
	{ "err",     LOG_ERR     },
	{ "error",   LOG_ERR     },
	{ "info",    LOG_INFO    },
	{ "notice",  LOG_NOTICE  },
	{ "warning", LOG_WARNING },
};

int spd_syslog_priority(const char *priority)
{
	int i;

	for(i = 0; i < ARRAY_LEN(priority_map); i++) {
		if(!strcasecmp(priority_map[i], priority)) {
			return priority_map[i].value;
		}
	}

	return -1;
}

const char *spd_syslog_priority_name(int priority)
{
	int i;
	
	for(i = 0; i < ARRAY_LEN(priority_map); i++) {
		if(priority_map[i].value == priority) {
			return priority_map[i].name;
		}
	}

	return NULL;
}

static const int logger_level_to_syslog_map[] = {
	[__LOG_DEBUG]   = LOG_DEBUG,
	[1]             = LOG_INFO, /* Only kept for backwards compatibility */
	[__LOG_NOTICE]  = LOG_NOTICE,
	[__LOG_WARNING] = LOG_WARNING,
	[__LOG_ERROR]   = LOG_ERR,
	[__LOG_VERBOSE] = LOG_DEBUG,
	[__LOG_DTMF]    = LOG_DEBUG,
};

int spd_syslog_loglevel_to_priority(int loglevel)
{
	if (loglevel < 0 || loglevel >= ARRAY_LEN(logger_level_to_syslog_map)) {
		return -1;
	}
	return logger_level_to_syslog_map[loglevel];
}