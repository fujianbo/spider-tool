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

#include "threadprivdata.h"
#include "utils.h"

void *spd_threadprivdata_get(struct spd_threadprivdata *ts, size_t init_size)
{
    void *buf;

    pthread_once(&ts->once, ts->init_key);
    if(!(buf = pthread_getspecific(ts->key))) {
        if(!(buf = spd_calloc(1, init_size)))
            return NULL;
        
        if(ts->init_custom && ts->init_custom(buf)) {
            spd_safe_free(buf);
            return NULL;
        }
        pthread_setspecific(ts->key, buf);
	}

    return buf;
}



struct spd_dynamic_str *spd_dynamic_str_create(size_t len)
{
    struct spd_dynamic_str *buffer;

    if(!(buffer = spd_calloc(1, sizeof(*buffer) + len)))
        return NULL;

    buffer->len = len;

    return buffer;
}

struct spd_dynamic_str *spd_dynamic_str_get(struct spd_threadprivdata *tpd, size_t len)
{
    struct spd_dynamic_str * buf;

    if(!(buf = spd_threadprivdata_get(tpd, sizeof(*buf) + len)))
        return NULL;
    if(!buf->len)
        buf->len = len;

    return buf;
}

int spd_thread_dynamic_str_build_helper(struct spd_dynamic_str **buf, size_t max_len,struct spd_threadprivdata *tpd, int append, 
		const char *fmt, va_list ap)
{
    int res;
	int offset = (append && (*buf)->len) ? strlen((*buf)->str) : 0;
    res = vsnprintf((*buf)->str + offset, (*buf)->len - offset, fmt, ap);

	/* Check to see if there was not enough space in the string buffer to prepare
	 * the string.  Also, if a maximum length is present, make sure the current
	 * length is less than the maximum before increasing the size. */
	if ((res + offset + 1) > (*buf)->len && (max_len ? ((*buf)->len < max_len) : 1)) {
		/* Set the new size of the string buffer to be the size needed
		 * to hold the resulting string (res) plus one byte for the
		 * terminating '\0'.  If this size is greater than the max, set
		 * the new length to be the maximum allowed. */
		if (max_len)
			(*buf)->len = ((res + offset + 1) < max_len) ? (res + offset + 1) : max_len;
		else
			(*buf)->len = res + offset + 1;

		if (!(*buf = spd_realloc(*buf, (*buf)->len + sizeof(*(*buf)))))
			return SPD_DNMCSTR_BUILD_FAILEDD;

		if (append)
			(*buf)->str[offset] = '\0';

		if (tpd) {
			pthread_setspecific(tpd->key, *buf);
            /* va_end() and va_start() must be done before calling
		    * vsnprintf() again. */
		}
	    	return SPD_DNMCSTR_BUILD_RETRY;
	}
    
}

int spd_dynamic_str_set(struct spd_dynamic_str **buf, size_t max_len,struct spd_threadprivdata *ts, const char *fmt, ...)
{
    int res;
	va_list ap;

	va_start(ap, fmt);
	res = spd_thread_dynamic_str_set_helper(buf, max_len, ts, fmt, ap);
	va_end(ap);

	return res;
}

int spd_dynamic_str_append(struct spd_dynamic_str **buf, size_t max_len,struct spd_threadprivdata *ts, const char *fmt, ...)
{
    int res;
    va_list ap;
    
    va_start(ap, fmt);
    res = spd_thread_dynamic_str_append_helper(buf, max_len, ts, fmt, ap);
    va_end(ap);

    return res;
}

