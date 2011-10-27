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

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "strings.h"


char spd_b10tob16(char c)
{
	return HEX[c & 15];
}

char *spd_strndup(const char *str, int size)
{
	char *ret= NULL;

	if(str && size) {
		int len = strlen(str);
		int rlen = size > len ? (len) : (size);
		ret = spd_calloc(1, rlen);
		memcpy(ret, str, rlen);
	}
	
	return ret;
}

char spd_b16tob10(char c)
{
	return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
}

int spd_is_digitstring(const char *s)
{
	while(s && *s) {
		if(*s < 48 || *s > 57) {
			return -1;
		}
		s++;
	}
	return 0;
}

char *spd_skip_blanks(const char *s)
{
	/* 32 stand for a blank */
	while(*s && ((unsigned char) *s) < 33)
		s++;
	return (char *)s;
}

char * spd_skip_nonblanks(char *s)
{
	while(*s && ((unsigned char) *s) > 32)
		s++;
	return (char *)s;
}

char * spd_trim_blanks(char *s)
{
	char *tmp = s;

	if(tmp) {
		tmp += strlen(s) - 1;

		/* It's tempting to only want to erase after we exit this loop, 
		   but since spd_trim_blanks *could* receive a constant string
		   (which we presumably wouldn't have to touch), we shouldn't
		   actually set anything unless we must, and it's easier just
		   to set each position to \0 than to keep track of a variable
		   for it */
		  while((tmp >= s) && ((unsigned char) *tmp) < 33)
		  	  *(tmp--) = '\0';
	}

	return s;
}

char *spd_strip(char *s)
{
    s = spd_skip_blanks(s); /* skip head */
	if(s)
		spd_trim_blanks(s); /* skip tail */

	return s;
}

char *spd_strip_quoted(char *s, const char *first_quote, const char *last_quote)
{
	char *pe;
	char *pb;

	s = spd_strip(s);   /* trim begin and tail blanks */

	if((pb = strchr(first_quote, *s)) && *pb != '\0'){
		pe = s + strlen(s) - 1;
		if(*pe ==*(last_quote + (pb - first_quote))) {
			s++;
			*pb = '\0';
		}
	}

	return s;
}

char * spd_unescape_semicolon(char *s)
{
	char *pe;
    char *pw = s;

    while((pe = strchr(pw, ';'))) {
        if((pe > pw) && (*(pe - 1) == '\\')) {
            memmove(pe -1, pe, strlen(pe) + 1);
            pw = pe;
        } else {
            pw = pe + 1;
        }
    }
}

void spd_copy_string(char *dst, const char *src, size_t size)
{
	while(*src && size) {
		*dst++ = *src++;
		size--;
	}

	if(__builtin_expect(!size, 0))
		dst--;
	*dst = '\0';
}

int spd_stringindex(const char *str, int size, const char *substr)
{
	if(str && substr) {
		const char *substart= strstr(str, substr);
		if(substart && substart < (size + str))
			return substart - str;
	}
	return -1;
}

int spd_strcontains(const char *str, int size, const char *substr)
{
	return (spd_stringindex(str, size, substr) >= 0);
}

void spd_strupdate(char **src, const char *newval)
{
	spd_safe_free(src);
	*src = spd_strdup(newval);
}

int spd_snprintf(char *buf, int  size, const char *fmt,...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vsnprintf(buf, size, fmt, ap);
	va_end(ap);

	return ret;
}

void spd_str_join(char *s, size_t len, const char *const w[])
{
    int x, ofs = 0;
    const char *src;

    if(!s)
        return;

    for(x = 0; ofs < len && w[x]; x++) {
        if(x > 0)
            s[ofs++] =' ';
        for(src = w[x]; *src && ofs < len; src++)
            s[ofs++] = *src;
    }
    if(ofs == len)
        ofs--;
    s[ofs] = '\0';
}
