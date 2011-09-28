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

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>

#include "linkedlist.h"
#include "logger.h"
#include "utils.h"
#include "threadprivdata.h"
#include "strings.h"
#include "lock.h"
#include "config.h"
#include "const.h"


#define MAX_HOSTLEN  80
#define MESSAGE   "message_log"

static char dateformat[256] = "%b %e %T";
static int filesize_reload_need = 0;   /* used indicate logger file size overload */
static FILE *message = NULL;
static int global_logmask = -1;

static char *loglevels[] = {
    "DEBUG",
    "NOTICE",
    "WARNING",
    "ERROR",
    "VERBOSE"
};

static int colors[] = {
	COLOR_BRGREEN,
	COLOR_BRBLUE,
	COLOR_YELLOW,
	COLOR_BRRED,
	COLOR_RED,
	COLOR_GREEN,
	COLOR_BRGREEN
};

/* spider log file */
struct {
    unsigned int message:1;
} spd_logfiles = {1};

static char hostname[MAX_HOSTLEN];

/* spider log type support */
enum spd_logtype {
    LOGTYPE_FILE,
    LOGTYPE_CONSOLE,
    LOGTYPE_SYSLOG,
};

struct spd_verb {
    void (*verboser)(const char *string);
    SPD_LIST_ENTRY(spd_verb) list;
};

static SPD_RWLIST_HEAD_STATIC(verbosers, spd_verb);

/* stand for a single log entry  */
struct spd_logchain {
    int facility;                  /* syslog facility */
    enum spd_logtype type;         /* where to log ? */
    int logmark;                   /* what log to chain, debug, notice or ... */
    FILE *fptr;                    /* LOGFILE type file ptr */
    char filename[256];            /* LOGFILE type filename */
    SPD_LIST_ENTRY(spd_logchain) list; 
};

static SPD_RWLIST_HEAD_STATIC(spd_logchains, spd_logchain);

/* thread specified dynamic data buf for verbose */
SPD_THREADPRIVDATA(verbose_buf);
#define VERBOSE_BUF_INIT_SIZE  128

/* thread specified dynamic data buf for log */
SPD_THREADPRIVDATA(log_buf);
#define LOG_BUF_INIT_SIZE  128

static void _handle_sigxfsz(int sig)
{
    filesize_reload_need = 1;
}

static struct sigaction handle_sigxfsz = {
    .sa_handler = _handle_sigxfsz,
    .sa_flags  = SA_RESTART,
};

static int make_components(char *s, int lineno)
{
    char *w;
    int res = 0;
    char *stringp = s;

    while((w = strsep(&stringp, ","))) {
        w = spd_strip(w);
        if(spd_strlen_zero(w)) {
            continue;
        }

        if (!strcasecmp(w, "error")) 
			res |= (1 << __LOG_ERROR);
		else if (!strcasecmp(w, "warning"))
			res |= (1 << __LOG_WARNING);
		else if (!strcasecmp(w, "notice"))
			res |= (1 << __LOG_NOTICE);
		else if (!strcasecmp(w, "debug"))
			res |= (1 << __LOG_DEBUG);
		else if (!strcasecmp(w, "verbose"))
			res |= (1 << __LOG_VERBOSE);
		else {
			fprintf(stderr, "Logfile Warning: Unknown keyword '%s' at line %d of logger.conf\n", w, lineno);
		}
	}

	return res;
}

static struct spd_logchain *make_logchain(char *channel, char *components, int lineno)
{
    struct spd_logchain *chan;
    char facility;

    if(spd_strlen_zero(channel) || !(chan = spd_calloc(1, sizeof(*chan))))
        return NULL;

    if(!strcasecmp(channel, "console")) {
        chan->type = LOGTYPE_CONSOLE;
    } else if (!strncasecmp(channel, "syslog", 6)) {
    	/*
    	       * TODO : support syslog
		* syntax is:
		*  syslog.facility => level,level,level
		*/
    }else {
        if(!spd_strlen_zero(hostname)) {
            snprintf(chan->filename, sizeof(chan->filename), "%s/%s.%s",
                channel[0] != '/' ? spd_config_SPD_LOG_DIR : "", channel, hostname);
        } else {
            snprintf(chan->filename, sizeof(chan->filename), "%s/%s", channel[0] != '/' ?
                 spd_config_SPD_LOG_DIR : "", channel);
          }
          chan->fptr = fopen(chan->filename, "a");
          if(!chan->fptr) {
             fprintf(stderr, "Logger Warning: Unable to open log file '%s': %s\n", chan->filename, strerror(errno));
          }
          chan->type = LOGTYPE_FILE;
     }
     chan->logmark = make_components(components, lineno);
	 
     return chan;
    
}

static void init_logger_chain(void)
{
    struct spd_logchain *logchain;
    struct spd_config *cfg;
    struct spd_variable *var;
    global_logmask = 0;
    const char *s;

    SPD_RWLIST_WRLOCK(&spd_logchains);
    while((logchain = SPD_RWLIST_REMOVE_HEAD(&spd_logchains, list)))
        free(logchain);
    SPD_RWLIST_UNLOCK(&spd_logchains);

	
    closelog();

    cfg = spd_config_load(spd_config_SPD_LOGGER_FILE);
	/* have config logger.conf  */
    if(cfg) { 
        if((s = spd_variable_retrive(cfg, "general", "appendhostname"))) {
            if(spd_true(s)) {
                if(gethostname(hostname, sizeof(hostname) - 1)) {
                    spd_copy_string(hostname, "unknown", sizeof(hostname));
                } 
            } else
                hostname[0] = '\0';
        } else 
            hostname[0] = '\0';
        if((s = spd_variable_retrive(cfg, "general", "dataformat")))
            spd_copy_string(dateformat, s, sizeof(dateformat));
        /*if((s = spd_variable_retrive(cfg, "general", "messsge")))
           		 spd_logfiles.message = spd_true(s);
            */
        SPD_RWLIST_WRLOCK(&spd_logchains);
        var = spd_variable_browse(cfg, "logfiles");
            for(; var; var = var->next) {
                if(!(logchain = make_logchain(var->name, var->value, var->lineno)))
                    continue;
                SPD_RWLIST_INSERT_HEAD(&spd_logchains,logchain, list);
                global_logmask |= logchain->logmark;
            }
            SPD_RWLIST_UNLOCK(&spd_logchains);

	   spd_mkdir(spd_config_SPD_LOG_DIR, 0755);
	   
       spd_config_destroy(cfg);
    }else{       /* no config file , we will use defualt settings */
		fprintf(stderr, "Unable to open logger.conf , default setting will be used\n");
		if(!(logchain = spd_calloc(1, sizeof(struct spd_logchain)))) {
			return;
		}
		logchain->type  = LOGTYPE_CONSOLE;
		logchain->logmark = __LOG_WARNING | __LOG_NOTICE | __LOG_ERROR;
		SPD_RWLIST_WRLOCK(&spd_logchains);
		SPD_RWLIST_INSERT_HEAD(&spd_logchains, logchain, list);
		global_logmask |= logchain->logmark;
		SPD_RWLIST_UNLOCK(&spd_logchains);

		return;
    }
}

int init_logger(void)
{
	char tmp[256];
	int res = 0;
    sigaction(SIGXFSZ, &handle_sigxfsz, NULL);
    
    init_logger_chain();
	/*
    	if(spd_logfiles.message) {
        spd_mkdir(spd_config_SPD_LOG_DIR, 0755);
        snprintf(tmp, sizeof(tmp), "%s%s", (char *)spd_config_SPD_LOG_DIR, MESSAGE);
        message = fopen((char *)tmp, "a");
        if(message) {
           spd_log(LOG_DEBUG, "Started Spider massage log\n\n");
            
        } else {
            spd_log(LOG_ERROR, "unable to create event log:%s\n", strerror(errno));
            res = -1;
        }
    	}*/

}

void close_logger()
{
	struct spd_logchain *log;
	
	SPD_RWLIST_WRLOCK(&spd_logchains);

	SPD_RWLIST_TRAVERSE(&spd_logchains, log, list) {
		if(log->fptr && log->fptr != STDOUT && log->fptr != STDERR) {
			fclose(log->fptr);
			log->fptr = NULL;
		}
	}
	closelog(); /* close system log */
	
	SPD_RWLIST_UNLOCK(&spd_logchains);
	
	return;
	
}
void spd_log(int level, const char *file, int line, const char *function, const char 
    *fmt, ...)
{
    struct spd_logchain *chan;
    struct spd_dynamic_str *buf;
    time_t t;
    struct tm *tm;;
    char date[256];

    va_list ap;

    if(!(buf = spd_dynamic_str_get(&log_buf, LOG_BUF_INIT_SIZE)))
        return;
    if(SPD_RWLIST_EMPTY(&spd_logchains)) {
        if(level != __LOG_VERBOSE) {
            int res;
            va_start(ap, fmt);
            res = spd_thread_dynamic_str_set_helper(&buf, BUFSIZ, &log_buf, fmt, ap);
            va_end(ap);
            if(res != SPD_DNMCSTR_BUILD_FAILEDD) {
                term_filter_escapes(buf->str);
                fputs(buf->str, stdout);
            }
        }
        return ;
    }
    
    t = time(NULL);
    tm = localtime(&t);
    memset(date, 0, sizeof(date));
    strftime(date, sizeof(date), dateformat, tm);
    SPD_RWLIST_WRLOCK(&spd_logchains);

	/*
       if(spd_logfiles.message) {
            va_start(ap, fmt);
            fprintf(message, "%s spider[%ld]: ", date, (long)getpid());
            vfprintf(message, fmt, ap);
            fflush(message);
            va_end(ap);
       }*/

    SPD_RWLIST_TRAVERSE(&spd_logchains, chan, list) {
    	/* console log */
        if((chan->logmark & (1 << level)) && (chan->type == LOGTYPE_CONSOLE)) {

            char linestr[128];
            char tmp1[80], tmp2[80], tmp3[80], tmp4[80];
            if(level != __LOG_VERBOSE) {
                int res;
                sprintf(linestr,"%d", line);
                spd_dynamic_str_set(&buf, BUFSIZ, &log_buf,
                    "[%s] %s[%ld]: %s:%s %s: ",
					date,
					term_color(tmp1, loglevels[level], colors[level], 0, sizeof(tmp1)),
					(long)getpid(),
					term_color(tmp2, file, COLOR_BRWHITE, 0, sizeof(tmp2)),
					term_color(tmp3, linestr, COLOR_BRWHITE, 0, sizeof(tmp3)),
					term_color(tmp4, function, COLOR_BRWHITE, 0, sizeof(tmp4)));
				/*filter to the console!*/
				term_filter_escapes(buf->str);
				//spd_console_puts_mutable(buf->str);
				fputs(buf->str, stdout);
				fflush(stdout);
				
                va_start(ap, fmt);
				res = spd_thread_dynamic_str_set_helper(&buf, BUFSIZ, &log_buf, fmt, ap);
				va_end(ap);
				if (res != SPD_DNMCSTR_BUILD_FAILEDD) {
					fputs(buf->str, stdout);
					fflush(stdout);
				}
                
            }
        }else if((chan->logmark & (1 << level)) && (chan->fptr)) {
        		int res;
			spd_dynamic_str_set(&buf, BUFSIZ, &log_buf, 
				"[%s] %s[%ld] %s: ",
				date, loglevels[level], (long)getpid(), file);
			res = fprinf(chan->fptr, "%s", buf->str);
			if(res <= 0 && !spd_strlen_zero(buf->str)) {
				fprintf(stderr,"****  Logging Error: ***********\n");
				if (errno == ENOMEM || errno == ENOSPC) {
					fprintf(stderr, " logging error: Out of disk space, can't log to log file %s\n", chan->filename);
				} else
					fprintf(stderr, "Logger Warning: Unable to write to log file '%s': %s (disabled)\n", chan->filename, strerror(errno));
			} else {
				va_start(ap, fmt);
				res = spd_thread_dynamic_str_set_helper(&buf, BUFSIZ, &log_buf, fmt, ap);
				va_end(ap);
				if(res != SPD_DNMCSTR_BUILD_FAILEDD) {
					fputs(buf->str, chan->fptr);
					fflush(chan->fptr);
				}
			}
         }
    }
    SPD_RWLIST_UNLOCK(&spd_logchains);

    if(filesize_reload_need) {
        //reload_logger(1);
        spd_log(LOG_DEBUG, "retated logs per sigxfsz \n");
        spd_verbose("retated log size \n");
    }
}

void spd_verbose(const char *fmt, ...)
{
    struct spd_verb *v;
    struct spd_dynamic_str *buf;
    int res;
    va_list ap;
    int i = 0;
    char *tmp = alloca(strlen(fmt) + 2);
    sprintf(tmp, "%c%s", 127, fmt);
    fmt = tmp;
    
    if(!(buf = spd_dynamic_str_get(&verbose_buf, VERBOSE_BUF_INIT_SIZE)))
        return;

    va_start(ap, fmt);

    res = spd_thread_dynamic_str_set_helper(&buf, 0, &verbose_buf, fmt, ap);
    va_end(ap);
    if(res == SPD_DNMCSTR_BUILD_FAILEDD)
        return ;
    term_filter_escapes(buf->str);

    SPD_RWLIST_RDLOCK(&verbosers);
    SPD_LIST_TRAVERSE(&verbosers, v, list)
        v->verboser(buf->str);
    SPD_RWLIST_UNLOCK(&verbosers);
    spd_log(LOG_VERBOSE, "%s", buf->str + 1);

}

int spd_register_verbose(void(*v)(const char *string))
{
    struct spd_verb *verb;

    if(!(verb = spd_malloc(sizeof(*verb))))
        return -1;

    verb->verboser = v;
    SPD_RWLIST_WRLOCK(&verbosers);
    SPD_RWLIST_INSERT_HEAD(&verbosers, verb, list);
    SPD_RWLIST_UNLOCK(&verbosers);
}

int spd_unregister_verbose(void(* verboser)(const char * string))
{
    struct spd_verb *cur;

    SPD_RWLIST_WRLOCK(&verbosers);
    SPD_RWLIST_TRAVERSE_SAFE_BEGIN(&verbosers, cur, list) {
        if(cur->verboser == verboser) {
            SPD_RWLIST_REMOVE_CURRENT(&verbosers, list);
            free(cur);
            break;
        }
    }
    SPD_LIST_TRAVERSE_SAFE_END
    SPD_RWLIST_UNLOCK(&verbosers);

    return cur ? 0 : -1;
}

