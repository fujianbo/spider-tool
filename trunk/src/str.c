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

#include "str.h"
#include "utils.h"


/*!
 * core handler for dynamic strings.
 * This is not meant to be called directly, but rather through the
 * various wrapper macros
 *	spd_str_set(...)
 *	spd_str_append(...)
 *	spd_str_set_va(...)
 *	spd_str_append_va(...)
 */

int __spd_str_helper(struct spd_str **buf, ssize_t max_len,
	int append, const char *fmt, va_list ap)
{
	int res, need;
	int offset = (append && (*buf)->len) ? (*buf)->used : 0;
	va_list aq;

	do {
		if(max_len < 0) {
			max_len = (*buf)->len;
		}

		/*
		 * Ask vsnprintf how much space we need. Remember that vsnprintf
		 * does not count the final <code>'\0'</code> so we must add 1.
		 */
		va_copy(aq,ap);
		res = vsnprintf((*buf)->buf + offset, (*buf)->len - offset, fmt, aq);
		need = res + offset + 1;
		/*
		 * If there is not enough space and we are below the max length,
		 * reallocate the buffer and return a message telling to retry.
		 */
		if(need > (*buf)->len && (max_len == 0 || (*buf)->len < max_len) ) {
			int len = (int)(*buf)->len;
			if (max_len && max_len < need) {	/* truncate as needed */
				need = max_len;
			} else if (max_len == 0) {	/* if unbounded, give more room for next time */
				need += 16 + need / 4;
			}
			if (0) {	/* debugging */
				spd_verbose("extend from %d to %d\n", len, need);
			}
			if (spd_str_make_space(buf, need)) {
				spd_verbose("failed to extend from %d to %d\n", len, need);
				va_end(aq);
				return SPD_STR_BUILD_FAILED;
			}
			(*buf)->buf[offset] = '\0';	/* Truncate the partial write. */

			/* Restart va_copy before calling vsnprintf() again. */
			va_end(aq);
			continue;
		}
		va_end(aq);
		break;
		
	} while(1);

	/* update space used, keep in mind the truncation */
	(*buf)->used = (res + offset > (*buf)->len) ? (*buf)->len - 1: res + offset;

	return res;
}

char *__spd_str_helper2(struct spd_str **buf, ssize_t maxlen, const char *src, size_t maxsrc, int append, int escapecommas)
{
	int dynamic = 0;
	char *ptr = append ? &((*buf)->buf[(*buf)->used]) : (*buf)->buf;

	if (maxlen < 1) {
		if (maxlen == 0) {
			dynamic = 1;
		}
		maxlen = (*buf)->len;
	}

	while (*src && maxsrc && maxlen && (!escapecommas || (maxlen - 1))) {
		if (escapecommas && (*src == '\\' || *src == ',')) {
			*ptr++ = '\\';
			maxlen--;
			(*buf)->used++;
		}
		*ptr++ = *src++;
		maxsrc--;
		maxlen--;
		(*buf)->used++;

		if ((ptr >= (*buf)->buf + (*buf)->len - 3) ||
			(dynamic && (!maxlen || (escapecommas && !(maxlen - 1))))) {
			char *oldbase = (*buf)->buf;
			size_t old = (*buf)->len;
			if (spd_str_make_space(buf, (*buf)->len * 2)) {
				/* If the buffer can't be extended, end it. */
				break;
			}
			/* What we extended the buffer by */
			maxlen = old;

			ptr += (*buf)->buf - oldbase;
		}
	}
	if (__builtin_expect(!maxlen, 0)) {
		ptr--;
	}
	*ptr = '\0';
	return (*buf)->buf;
}
