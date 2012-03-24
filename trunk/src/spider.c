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
#include<sys/poll.h>

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
static char *spd_config_SPD_PID = "/var/run/spider.pid";
static char *spd_config_SPD_SOCKET = "/var/run/spider.socket";

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
		 if(setsid()< 0) {
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

	/* Change the file mode mask */
    umask(0);
	
	chdir("/");
	
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

static pthread_t lthread;

static void network_verboser(const char *s)
{
	spd_network_puts_mutable(s);
}
static void *netconsole_thread(void *nconsole)
{
	struct console *conn = nconsole;

	char hostname[MAXHOSTNAMELEN] = "";
	char tmp[512];
	int res;
	struct pollfd fds[2];
	
	if(gethostname(hostname, sizeof(hostname) -1))
		spd_copy_string(hostname, "<Unknown>", sizeof(hostname));
	snprintf(tmp, sizeof(tmp), "%s/%ld/%f\n", hostname, (long)spd_mainpid, SPD_VERSION);
	fdprint(conn->fd, tmp);
	for(;;) {
		fds[0].fd = conn->fd;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		fds[1].fd = conn->p[0];
		fds[1].events = POLLIN;
		fds[1].revents = 0;
		res = poll(fds, 2, -1);
		if(res < 0) {
			if(errno != EINTR)
				spd_log(LOG_WARNING, "poll returned < 0: %s\n", strerror(errno));
			continue;
		}
		if(fds[0].revents) {
			res = read(conn->fd, tmp, sizeof(tmp));
			if(res < 1) {
				break;
			}
			tmp[res] = 0;
			spd_cli_command(conn->fd, tmp);
		}
		if(fds[1].revents) {
			res = read(conn->p[0], tmp, sizeof(tmp));
			if(res < 1) {
				spd_log(LOG_ERROR, "read returned : %d\n", res);
				break;
			}
			res = write(conn->fd, tmp,res);
			if(res < 1)
				break;
		}	
	}
	if(option_verbose > 2)
		spd_verbose(VERBOSE_PREFIX_3 "Remote UNIX connection disconnected\n");
	close(conn->fd);
	close(conn->p[0]);
	close(conn->p[1]);
	conn->fd = -1;
	
}
static void *listener_thread(void *nothing)
{
	struct sockaddr_un sunaddr;
	int s;
	socklen_t len;
	int x;
	int flags;
	struct pollfd fds[1];
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	for(;;) {
		if(spd_socket  < 0)
			return NULL;

		fds[0].fd = spd_socket;
		fds[0].events = POLLIN;
		s = poll(fds, 1, -1);
		pthread_testcancel();

		if(s < 0) {
			if(errno != EINTR)
				spd_log(LOG_WARNING, "poll returned error: %s\n", strerror(errno));
			continue;
		}

		s = accept(spd_socket, (struct sockaddr *)&sunaddr, &len);

		if(s < 0) {
			if(errno != EINTR) {
				spd_log(LOG_WARNING, " accept return error : %s \n", strerror(errno));
			continue;
			}
		} else {
			for(x = 0; x < SPD_MAX_CONNECTS; x++) {
				if(consoles[x].fd < 0) {
					if(socketpair(AF_UNIX, SOCK_STREAM, 0, consoles[x].p)) {
						spd_log(LOG_ERROR, "Unable to create pipe : %s\n", strerror(errno));
						consoles[x].fd = -1;
						fdprint(s, "Server failed to create pipe\n");
						close(s);
						break;
					}

					flags = fcntl(consoles[x].p[1], F_GETFL);
					fcntl(consoles[x].p[1], F_SETFL, flags | O_NONBLOCK);
					consoles[x].fd = s;
					consoles[x].mute = spd_opt_mute;
					if(spd_pthread_create_background(&consoles[x].t, &attr, netconsole_thread, &consoles[x])) {
						spd_log(LOG_ERROR, "Unable to spawn thread to handler connection : %s\n", strerror(errno));
						close(consoles[x].p[0]);
						close(consoles[x].p[1]);
						consoles[x].fd = -1;
						close(s);
					}
					break;
				}
			}
			if(x >= SPD_MAX_CONNECTS) {
				fdprint(s, "No more connect \n");
				spd_log(LOG_WARNING, "No more connect allowd \n");
				close(s);
			} else if(consoles[x].fd > 0) {
				if(option_verbose)
				spd_verbose(VERBOSE_PREFIX_3 "remote UNIX connect\n");
			}
		}

		
		
	}
}
static int start_domainserver()
{
	struct sockaddr_un sunaddr;
	int res;
        int x;

	for (x = 0; x < SPD_MAX_CONNECTS; x++)	
		consoles[x].fd = -1;
	unlink(spd_config_SPD_SOCKET);
	spd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if(spd_socket < 0) {
		spd_log(LOG_WARNING, " failed to create socket for domainserver \n", strerror(errno));
		return -1;
	}

	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	spd_copy_string(sunaddr.sun_path, spd_config_SPD_SOCKET, sizeof(sunaddr.sun_path));
	res = bind(spd_socket, (struct sockaddr *)&sunaddr, sizeof(sunaddr));
	if(res) {
		spd_log(LOG_WARNING, "Unable to bind socket to %s : %s\n", spd_config_SPD_SOCKET, strerror(errno));
		close(spd_socket);
		spd_socket = -1;
		return -1;
	}

	res = listen(spd_socket, 2);
	if(res < 0) {
		spd_log(LOG_WARNING, "Unable to listen on socket %s : %s\n", spd_config_SPD_SOCKET, strerror(errno));
		close(spd_socket);
		spd_socket = -1;
		return -1;
	}

	spd_register_verbose(network_verboser);
	spd_pthread_create_background(&lthread, NULL, listener_thread, NULL);
	
	return 0;
}
static int spd_tryconn(void)
{
	struct sockaddr_un sunaddr;
	int res;
	spd_consock = socket(AF_UNIX, SOCK_STREAM, 0);
	if(spd_consock < 0) {
		spd_log(LOG_WARNING, "Unable to create socket: %s\n", strerror(errno));
		return 0;
	}
	memset(&sunaddr, 0, sizeof(sunaddr));
	spd_copy_string(sunaddr.sun_path, (char *)spd_config_SPD_SOCKET, sizeof(sunaddr.sun_path));
	res = connect(spd_consock, (struct sockaddr *)&sunaddr, sizeof(sunaddr));

	if(res) {
		close(spd_consock);
		spd_consock = -1;
		return 0;
	} else 
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
	FILE *f;
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
				spd_set_flag(&spd_options, SPD_OPT_FLAG_CONSOLE |SPD_OPT_FLAG_NO_FORK );
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

	if(spd_tryconn()) {
		
	}

	/* run in background  */
	if(!spd_opt_no_fork) {
		deamonize();
		//daemon(0,0);
		spd_mainpid = getpid();
		unlink(spd_config_SPD_PID);
		f = fopen(spd_config_SPD_PID, "w");
		if(f) {
			fprintf(f, "%ld\n", (long)spd_mainpid);
			fclose(f);
		} else {
			spd_log(LOG_WARNING, "cant not open spdier pid file  %s\n", strerror(errno));
		}
	}
	
	/* we start domain socket server every time the server start inorder
	    to support remote connection 
	*/
	start_domainserver();
	
	/* init logger engine */
    init_logger();
    spd_log(LOG_DEBUG, " welcome\n");
	spd_log(LOG_NOTICE, "sys name : %s debug level: %d  verbose level: %d\n",
            spd_config_SPD_SYSTEM_NAME, option_debug, option_verbose);

	for(;;) {
		sleep(1);
		spd_log(LOG_DEBUG, " welcome\n");
	}
}

 
