/*
 * Spider -- An open source xxx toolkit.
 *
 * Copyright (C) 2011 , Inc.
 *
 * lidp <openser@yeah.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

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
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "linkedlist.h"
#include "lock.h"
#include "threadprivdata.h"
#include "spider.h"
#include "config.h"
#include "logger.h"
#include "options.h"
#include "const.h"
#include "socket.h"
#include "thread.h"
#include "time.h"
#include "io.h"
#include "obj.h"
#include "cli.h"

/* max remote concol conn */
#define SPD_MAX_CONNECTS  128
/* \brief Welcome msg when starting cli interface */

#define WELCOME_MESSAGE \ 
	spd_verbose("             \n\n\t\tspider  0.1 copyright(c) 2011  ti-net inc\n"); \
	spd_verbose("                creat by lidp  lidp@ti-net.com.cn\n\n\n");


#define PIDFILE    "spider.pid"
static char *pfile = PIDFILE;

struct spd_flags spd_options ;

static float SPD_VERSION = 0.1;

static int spd_socket = -1;
static int spd_consock = -1;
static int spd_mainpid;
static char *old_argv[256];

//extern option_debug;
//extern option_verbose;

struct console {
	int fd;
	int p[2];
	pthread_t t;
	int mute;   /* is console mute for logs */
};

struct console consoles[SPD_MAX_CONNECTS];

struct spd_atexit {
    void (*func)(void);
    SPD_LIST_ENTRY(spd_atexit) list; 
};

static SPD_LIST_HEAD_STATIC(atexits, spd_atexit);


char spd_config_SPD_SYSTEM_NAME[PATH_MAX];
char spd_global_dirs_PID_DIR[PATH_MAX] = "/var/run";

int spd_register_atexit(void (*func)(void))
{
    struct spd_atexit *ae;
    
    if(!(ae = spd_calloc(1, sizeof(*ae)))) {
        return -1;
    }
    
    ae->func = func;
    
    spd_unregister_atexit(func);
    
    SPD_LIST_LOCK(&atexits);
    SPD_LIST_INSERT_HEAD(&atexits, ae, list);
    SPD_LIST_UNLOCK(&atexits);

    return 0;
}

void spd_unregister_atexit(void(*func)(void))
{
    struct spd_atexit *ae = NULL;
    
    SPD_LIST_LOCK(&atexits);
    SPD_LIST_TRAVERSE_SAFE_BEGIN(&atexits, ae, list) {
        if(ae->func == func) {
            SPD_LIST_REMOVE_CURRENT(&atexits, list);
            break;
        }
    }
    SPD_LIST_TRAVERSE_SAFE_END;
    SPD_LIST_UNLOCK(&atexits);
    
    if(ae) {
        spd_safe_free(ae);
    }
}
static char *handle_show_version(struct spd_cli_entry *e, int cmd, struct spd_cli_arg *arg)
{
	switch(cmd) {
		case CLI_USAGE:
			e->command = "core show version";
			e->usage = " Usage: core show version\n"
					" show spider version information.\n";
			return NULL;
		case CLI_COMPLETE:
			return NULL;	
	}

	if(arg->argc != 3)
		return CLI_SHOWUSAGE;

	spd_cli(arg->fd, "spider version %f\n", SPD_VERSION);

	return CLI_SUCCESS;
}

static struct spd_cli_entry spider_cli[] = {
	SPD_CLI_DEFINE(handle_show_version, "Display version info"),
};

static int show_usage()
{
	printf("Spider , Copyright lidp 2011 inc.\n");
	printf("Usage spider [options] \n");
	return 0;
}
static int show_version()
{
	printf(" spider version  %f \n", SPD_VERSION);
	return 0;
}

static void spd_set_global_dirs()
{
	
}

static void deamonize()
{
	int fd;
	int pid;

	if(pid = fork()) {
		exit(0); /*  parent process, exit it */
	} else if(pid < 0) {
		spd_log(LOG_ERROR, "error when fork\n");
		exit(0); /* error */
	} else {
		if(setsid() < 0) {
			spd_log(LOG_ERROR, "error when change session id");
			exit(0);
		}
	}
	
	/* child proc, for now , we are  session leader  and process group  leader */
	if(pid = fork()) {
		exit(0); /* kill child */
	} else if(pid < 0) {
		spd_log(LOG_ERROR, "error when fork\n");
		exit(0);
	} 

	/* child child proc, we are no longer the  session leader  and process group leader */
	/* ignore fd leak */
	fd = open("/dev/null", O_RDONLY);
	if(fd != 0) {
		dup2(fd, 0);
		close(fd);
	}
	fd = open("/dev/null", O_WRONLY);
	if(fd != 1) {
		dup2(fd, 1);
		close(fd);
	}
	fd = open("/dev/null", O_WRONLY);
	if(fd != 2) {
		dup2(fd, 2);
		close(fd);
	}
	
}
static int kill_spider_backgroud()
{
	char path[PATH_MAX];
	FILE *f;
	int pid = 0; /* pid number from pid file */

	/* set global dirs */

	spd_snprintf(path, sizeof(path),"%s/%s", spd_global_dirs_PID_DIR, pfile);

	if((f = fopen(path, "r")) == 0) {
		fprintf(stderr, "Can't open pid file %s", path);
		return -1;
	}
	if(fscanf(f, "%d", &pid)!= 1) {
		spd_log(LOG_ERROR, "unenble to get pid !\n");
	}
	
	/* send signal SIGTERM to kill */
	if(pid > 0) {
		spd_log(LOG_DEBUG, "Killing  %d\n", pid);
		kill(pid, SIGTERM);
	}

	fclose(f);

	return 0;
}

/* Sending messages from the daemon back to the display requires _excluding_ the terminating NULL */
static int fdprint(int fd, const char *s)
{
	return write(fd, s, strlen(s));
}

/*!
 * \brief log the string to all attached console clients
 */
static void spd_network_puts_mutable(const char *string)
{
	int x;
	for (x = 0;x < SPD_MAX_CONNECTS; x++) {
		if (consoles[x].mute)
			continue;
		if (consoles[x].fd > -1) 
			fdprint(consoles[x].p[1], string);
	}
}

/*!
 * \brief log the string to the console, and all attached
 * console clients
 */
void spd_console_puts_mutable(const char *string)
{
	fputs(string, stdout);
	fflush(stdout);
	spd_network_puts_mutable(string);
}

static const char *fix_header(char *outbuf, int maxout, const char *s, char *cmp)
{
	const char *c;
	if (!strncmp(s, cmp, strlen(cmp))) {
		c = s + strlen(cmp);
		term_color(outbuf, cmp, COLOR_GRAY, 0, maxout);
		return c;
	}
	return NULL;
}

static void console_verboser(const char *s)
{
	char tmp[80];
	const char *c = NULL;

	if ((c = fix_header(tmp, sizeof(tmp), s, VERBOSE_PREFIX_4)) ||
	    (c = fix_header(tmp, sizeof(tmp), s, VERBOSE_PREFIX_3)) ||
	    (c = fix_header(tmp, sizeof(tmp), s, VERBOSE_PREFIX_2)) ||
	    (c = fix_header(tmp, sizeof(tmp), s, VERBOSE_PREFIX_1))) {
		fputs(tmp, stdout);
		fputs(c, stdout);
	} else {
		if (*s == 127) {
			s++;
		}
		fputs(s, stdout);
	}

	fflush(stdout);
	/* Wake up a poll()ing console */
}


static void read_mainconfig(void)
{
	struct spd_config *cfg;
	struct spd_variable *v;
	
	cfg = spd_config_load(spd_config_maincfg);
	if(!cfg) {
		spd_log(LOG_WARNING, "Unable to open specifd main config file '%s'\n", spd_config_maincfg);
		return;
	}

	for(v = spd_variable_browse(cfg, "options"); v; v = v->next) {
		/* verbose level  (-v start up ) */
		if(!strcasecmp(v->name, "verbose")) {
			option_verbose = atoi(v->value);
		}
		else if (!strcasecmp(v->name, "timestamp")) {
			spd_set2_flag(&spd_options, spd_true(v->value), SPD_OPT_FLAG_TIMESTAMP);
		}
		else if(!strcasecmp(v->name, "debug")) {
			option_debug = 0;
			if(sscanf(v->value, "%30d", &option_debug) != 1) {
				option_debug = spd_true(v->value);

			}
		}
		else if(!strcasecmp(v->name, "systemname")) {
			spd_copy_string(spd_config_SPD_SYSTEM_NAME, v->value, sizeof(spd_config_SPD_SYSTEM_NAME));
		}
	}
	spd_config_destroy(cfg);
}

/*
 *  http://student.zjzk.cn/course_ware/data_structure/web/paixu/paixu8.2.1.1.htm
 */
static void insersion_sort(int array[], int len);
static void insersion_sort(int array[], int len)
{
    int i, j,key;
    for(j = 1; j < len; j++) {
	spd_log(LOG_DEBUG, "%d , %d, %d, %d, %d\n ", array[0], array[1], array[2],array[3],array[4]);
	key = array[j];
        i = j - 1;
	while(i >= 0 && array[i] > key) {
	    array[i+1] = array[i];
	    i--;
	}
	array[i+1] = key;
    }
    spd_log(LOG_DEBUG, "%d , %d, %d, %d, %d \n", array[0], array[1], array[2],array[3],array[4]);
}

/*
  * binary search
  */
static int binary_search(int array[], int len, int key)
{
	int left = 0, mid, end;

	end = len - 1;
	while(left < end ) {
		mid = ( left + end )/2;
		if(key > array[mid])
			left = mid + 1;
		else if(key < array[mid])
			end = mid - 1;
		else if (key = array[mid])
			return mid;
	}
	
	return -1;
}

static void term_handler()
{
	spd_log(LOG_NOTICE, "server term\n");
}

/* Main entry point */
int main(int argc, char *argv[])
{
    int x;
    int c;
	int is_deamon = 0;
    char hostname[MAXHOSTNAMELEN];
    char filename[80];
 	struct rlimit limit;
	struct sigaction term_action; 
	
    if(argc > sizeof(old_argv) / sizeof(old_argv[0]) -1) {
        fprintf(stderr, "truncationg argument size to %d\n", (int)(sizeof(old_argv) / sizeof(old_argv[0])) -1 );
        argc = sizeof(old_argv) / sizeof(old_argv[0]) -1;
    }

    for(x = 0; x < argc; x++)
        old_argv[x] = argv[x];
    old_argv[x] = NULL;

    if(gethostname(hostname, sizeof(hostname) -1)) 
		spd_copy_string(hostname, "<Unknown>", sizeof(hostname));
    	
	if(argv[0] && (strstr(argv[0], "rspider")) != NULL) {
		spd_set_flag(&spd_options, SPD_OPT_FLAG_REMOTE | SPD_OPT_FLAG_NO_FORK);
	}

	spd_mainpid = getpid();
	if(getenv("HOME"))
		snprintf(filename, sizeof(filename), "%s/.spider_history", getenv("HOME"));

	while((c = getopt(argc, argv, "cgvVrRhkd")) != -1) {
		switch (c) {
			case 'g':
				spd_set_flag(&spd_options, SPD_OPT_FLAG_CORE_DUMP);
				break;
			case 'h':
				show_usage();
				exit(0);
			case 'V':
				show_version();
				exit(0);
				break;
			case 'v':
				option_verbose++;
				break;
			case 'k':
				kill_spider_backgroud();
				exit(0);
			case 'd':
				option_debug++;
				break;
			case 'r':
				spd_set_flag(&spd_options, SPD_OPT_FLAG_NO_FORK |SPD_OPT_FLAG_REMOTE);
				break;
			case 'R':
				spd_set_flag(&spd_options, SPD_OPT_FLAG_NO_FORK| SPD_OPT_FLAG_RECONNECT);
			case 'c':
				spd_set_flag(&spd_options, SPD_OPT_FLAG_CONSOLE);
		}
	}

	/* catch TERM and INT signals */
	memset(&term_action, 0, sizeof(term_action));
	term_action.sa_handler = term_handler;
	sigaction(SIGINT,&term_action, NULL);
	sigaction(SIGTERM,&term_action, NULL);

	if(spd_opt_console || option_verbose || spd_opt_remote) {
        if(spd_register_verbose(console_verboser)) {
			spd_log(LOG_WARNING, "Unable to regster verbose \n");
		}
        WELCOME_MESSAGE;
     }
	if(spd_opt_console) {
		spd_verbose("[Booting...\n");
		spd_verbose("[ Reading Master Configuration ]\n");
	}

	read_mainconfig();

	for (x = 0; x < SPD_MAX_CONNECTS; x++)	
		consoles[x].fd = -1;

	/* in debug mode */
	if(spd_opt_core_dump) {
	 	memset(&limit, 0, sizeof(limit));
	 	limit.rlim_cur = RLIM_INFINITY;
	 	limit.rlim_max = RLIM_INFINITY;
	 	if(setrlimit(RLIMIT_CORE, &limit)) {
			spd_log(LOG_WARNING, " unable to disable core size res limit %s\n", strerror(errno));
	 	}

	 	if(getrlimit(RLIMIT_CORE, &limit)) {
			spd_log(LOG_WARNING, "unable to check rlimit of fd%s\n", strerror(errno));
		}
	}

	/* catch TERM and INT signals */


	 /* init logger engine */
     init_logger();
     spd_log(LOG_DEBUG, " welcome\n");
	 spd_log(LOG_NOTICE, "sys name : %s debug level: %d  verbose level: %d\n",
            spd_config_SPD_SYSTEM_NAME, option_debug, option_verbose);
	 
	 if(is_deamon) {
	 	spd_log(LOG_NOTICE, "run in background mode\n");
	 	deamonize();
	 }

	 for(;;);
}

 
