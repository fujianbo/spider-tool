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
 * \ref 
 */

#include "logger.h"

/* log level definition */
  const  char *loglevels[] = {
    "VERBOSE",
    "DEBUG",
    "NOTICE",
    "WARNING",
    "ERROR",
};

/* used by term_color */
  int colors[] = {
	COLOR_GREEN,
	COLOR_BRGREEN,
	COLOR_YELLOW,
	COLOR_BRRED,
	COLOR_RED,
	COLOR_BRBLUE,
	COLOR_BRGREEN,
};

/* date format , example : Jan  2 04:55:55*/
  char dateformat[256] = "%b %e %T";

void print_time()
{
    struct timeval tv;
    struct tm* ptm;
    char time_str[128];
    long milliseconds;
    gettimeofday(&tv, NULL);
    ptm = localtime(&tv.tv_sec);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", ptm);
    milliseconds = tv.tv_sec/1000;
    printf("%s.%03ld\n", time_str, milliseconds);
}

void tsk_copy_string(char *dst, const char *src, size_t size)
{
    while(*src && size) {
        *dst++ = *src++;
        size--;
    }

    if(__builtin_expect(!size, 0))
        dst--;
    *dst = '\0';
}

void tsk_console_puts(char *str)
{
	fprintf(stderr,"%s", str);
	fflush(stderr);
}

void term_filter_escapes(char *line)
{
	int i;
	int len = strlen(line);

	for (i = 0; i < len; i++) {
		if (line[i] != ESC)
			continue;
		if ((i < (len - 2)) &&
		    (line[i + 1] == 0x5B)) {
			switch (line[i + 2]) {
		 	case 0x30:
			case 0x31:
			case 0x33:
				continue;
			}
		}
		/* replace ESC with a space */
		line[i] = ' ';
	}
}

char *term_color(char *outbuf, const char *inbuf, int fgcolor, int bgcolor, int maxout)
{
	int attr=0;
	char tmp[40];

	memset(outbuf, 0, maxout);
		
	if (!fgcolor && !bgcolor) {
		tsk_copy_string(outbuf, inbuf, maxout);
		return outbuf;
	}
	if ((fgcolor & 128) && (bgcolor & 128)) {
		/* Can't both be highlighted */
		tsk_copy_string(outbuf, inbuf, maxout);
		return outbuf;
	}
	if (!bgcolor)
		bgcolor =COLOR_BLACK;

	if (bgcolor) {
		bgcolor &= ~128;
		bgcolor += 10;
	}
	if (fgcolor & 128) {
		attr = ATTR_BRIGHT;
		fgcolor &= ~128;
	}
	if (fgcolor && bgcolor) {
		snprintf(tmp, sizeof(tmp), "%d;%d", fgcolor, bgcolor);
	} else if (bgcolor) {
		snprintf(tmp, sizeof(tmp), "%d", bgcolor);
	} else if (fgcolor) {
		snprintf(tmp, sizeof(tmp), "%d", fgcolor);
	}
	if (attr) {
		snprintf(outbuf, maxout, "%c[%d;%sm%s%c[0;%d;%dm", ESC, attr, tmp, inbuf, ESC, 
    COLOR_WHITE, COLOR_BLACK + 10);
	} else {
		snprintf(outbuf, maxout, "%c[%sm%s%c[0;%d;%dm", ESC, tmp, inbuf, ESC, COLOR_WHITE, 
    COLOR_BLACK + 10);
	}
	return outbuf;
}

void  _tsk_log(int level, const char *file, int line, const char *function, const char *fmt, ...)
{

    if(level > LOGLEVEL){
      return;
    }
    time_t t;
    struct tm *tm;
    char date[64];
    char linestr[64];
    char tmp1[64];
    char buf[256];
    char buf1[256];
    char msg[512];
    int res;
    va_list ap;

    t = time(NULL);
    tm = localtime(&t);
    memset(date, 0, sizeof(date));
    memset(buf, 0, sizeof(buf));
    memset(buf1, 0, sizeof(buf1));
    memset(msg, 0, sizeof(msg));

    strftime(date, sizeof(date), dateformat, tm);
    snprintf(linestr,sizeof(linestr),"%d", line);
    snprintf(buf,sizeof(buf),"[%s] %s[%ld]: %s:%s %s: ",date,term_color(tmp1, loglevels[level], colors[level], 0, sizeof(tmp1)),(long)getpid(),
                                        file, linestr, function);
    term_filter_escapes(buf);

    va_start(ap, fmt);
    res = vsnprintf(buf1,sizeof(buf1) - 1,fmt, ap);
    va_end(ap);
		
    snprintf(msg, sizeof(msg) -1, "%s%s",buf,buf1);
	printf("%s", msg);

}


