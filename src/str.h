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

#ifndef _SPIDER_STR_H
#define _SPIDER_STR_H

#if defined (__cplusplus)||defined (c_plusplus)
extern "C"
{
#endif


#include "threadprivdata.h"
#include "utils.h"


/*!
 * \brief Error codes from __spd_str_helper()
 * The undelying processing to manipulate dynamic string is done
 * by __spd_str_helper(), which can return a success or a
 * permanent failure (e.g. no memory).
 */
enum {
	/*! An error has occurred and the contents of the dynamic string
	 *  are undefined */
	SPD_STR_BUILD_FAILED = -1,
	/*! The buffer size for the dynamic string had to be increased, and
	 *  __spd_str_helper() needs to be called again after
	 *  a va_end() and va_start().  This return value is legacy and will
	 *  no longer be used.
	 */
	SPD_STR_BUILD_RETRY = -2,
};

/*!
 * Support for dynamic strings.
 *
 * A dynamic string is just a C string prefixed by a few control fields
 * that help setting/appending/extending it using a printf-like syntax.
 *
 * One should never declare a variable with this type, but only a pointer
 * to it, e.g.
 *
 *	struct spd_str *ds;
 *
 * The pointer can be initialized with the following:
 *
 *	ds = spd_str_create(init_len);
 *		creates a malloc()'ed dynamic string;
 *
 *	ds = spd_str_alloca(init_len);
 *		creates a string on the stack (not very dynamic!).
 *
 *	ds = spd_str_thread_get(ts, init_len)
 *		creates a malloc()'ed dynamic string associated to
 *		the thread-local storage key ts
 *
 * Finally, the string can be manipulated with the following:
 *
 *	spd_str_set(&buf, max_len, fmt, ...)
 *	spd_str_append(&buf, max_len, fmt, ...)
 *
 * and their varargs variant
 *
 *	spd_str_set_va(&buf, max_len, ap)
 *	spd_str_append_va(&buf, max_len, ap)
 *
 * \param max_len The maximum allowed capacity of the spd_str. Note that
 *  if the value of max_len is less than the current capacity of the
 *  spds_str (as returned by spd_str_size), then the parameter is effectively
 *  ignored.
 * 	0 means unlimited, -1 means "at most the available space"
 *
 * \return All the functions return <0 in case of error, or the
 *	length of the string added to the buffer otherwise. Note that
 *	in most cases where an error is returned, characters ARE written
 *	to the spd_str.
 */

/*! \brief The descriptor of a dynamic string
 *  XXX storage will be optimized later if needed
 * We use the ts field to indicate the type of storage.
 * Three special constants indicate malloc, alloca() or static
 * variables, all other values indicate a
 * struct spd_threadprivdata  pointer.
 */

struct spd_str {
	size_t len;
	size_t used;
	struct spd_threadprivdata *str_tpv;
	#define STR_MALLOC ((struct spd_threadprivdata *)1)
	#define STR_ALLOCA ((struct spd_threadprivdata *)2)
	char buf[0];
	
};

/*!
 * \brief Create a malloc'ed dynamic length string
 *
 * \param init_len This is the initial length of the string buffer
 *
 * \return This function returns a pointer to the dynamic string length.  The
 *         result will be NULL in the case of a memory allocation error.
 *
 * \note The result of this function is dynamically allocated memory, and must
 *       be free()'d after it is no longer needed.
 */
static inline struct spd_str *spd_str_create(size_t init_len)
{
	struct spd_str *str;
	str = (struct spd_str *)spd_calloc(1, sizeof(*str) + init_len);
	if(str == NULL)
		return NULL;
	str->len = init_len;
	str->used = 0;
	str->str_tpv = STR_MALLOC;
	
	return str;	
}

/*! \brief Reset the content of a dynamic string.
 * Useful before a series of spd_str_append.
 */
static inline void spd_str_reset(struct spd_str *buf)
{
	if(buf) {
		buf->used = 0;
		if(buf->len) {
			buf->buf[0] = '\0';
		}
	}
}

/*! \brief Update the length of the buffer, after using spd_str merely as a buffer.
 *  \param buf A pointer to the spd_str string.
 */
static inline void spd_str_updata(struct spd_str *buf)
{
	buf->used = strlen(buf->buf);
}

/*!\brief Returns the current length of the string stored within buf.
 * \param buf A pointer to the spd_str structure.
 */
static inline size_t spd_str_len(struct spd_str *buf)
{
	return buf->used;
}

/*!\brief Returns the current maximum length (without reallocation) of the current buffer.
 * \param buf A pointer to the spd_str structure.
 * \retval Current maximum length of the buffer.
 */
static inline size_t spd_str_size(struct spd_str *buf)
{
	return buf->len;
}

/*!\brief Returns the string buffer within the ast_str buf.
 * \param buf A pointer to the spd_str structure.
 * \retval A pointer to the enclosed string.
 */
static inline char *spd_str_buffer(struct spd_str *buf)
{
	return (char *)buf->buf;
}

/*!\brief Truncates the enclosed string to the given length.
 * \param buf A pointer to the ast_str structure.
 * \param len Maximum length of the string.
 * \retval A pointer to the resulting string.
 */
static inline char *spd_str_truncate(struct spd_str *buf, ssize_t len)
{
	if(len < 0) {
		buf->used += ((ssize_t) abs(len)) > (ssize_t) buf->used ? -buf->used : len;
	}else { 
		buf->used = len;
	}
	buf->buf[buf->used] = '\0';

	return (char *)buf->buf;
}

/*!
 * Make space in a new string (e.g. to read in data from a file)
 */
static inline int spd_str_make_space(struct spd_str **buf, size_t new_len)
{
	struct spd_str *oldbuf = *buf;
	
	if((*buf)->len <= new_len)
		return 0;

	if((*buf)->str_tpv == STR_ALLOCA)
		return -1;

	*buf = (struct spd_str *)spd_realloc(*buf, sizeof(struct spd_str *) + new_len);
	if((*buf) == NULL){
		*buf = oldbuf;
		return -1;
	}
	
	if((*buf)->str_tpv != STR_MALLOC)
		pthread_setspecific((*buf)->str_tpv->key, *buf);

	(*buf)->len = new_len;
}

#define spd_str_alloca(init_len)			\
	({						\
		struct spd_str *__spd_str_buf;			\
		__spd_str_buf = alloca(sizeof(*__spd_str_buf) + init_len);	\
		__spd_str_buf->len = init_len;			\
		__spd_str_buf->used = 0;				\
		__spd_str_buf->str_tpv = DS_ALLOCA;			\
		__spd_str_buf->buf[0] = '\0';			\
		(__spd_str_buf);					\
	})

/*!
 * \brief Retrieve a thread locally stored dynamic string
 *
 * \param ts This is a pointer to the thread storage structure declared by using
 *      the SPD_THREADPRIVDATA macro.  If declared with 
 *      SPD_THREADPRIVDATA(my_buf, my_buf_init), then this argument would be 
 *      (&my_buf).
 * \param init_len This is the initial length of the thread's dynamic string. The
 *      current length may be bigger if previous operations in this thread have
 *      caused it to increase.
 *
 * \return This function will return the thread locally stored dynamic string
 *         associated with the thread storage management variable passed as the
 *         first argument.
 *         The result will be NULL in the case of a memory allocation error.
 *
 * Example usage:
 * \code
 * SPD_THREADPRIVDATA(my_str, my_str_init);
 * #define MY_STR_INIT_SIZE   128
 * ...
 * void my_func(const char *fmt, ...)
 * {
 *      struct spd_str *buf;
 *
 *      if (!(buf = spd_str_thread_get(&my_str, MY_STR_INIT_SIZE)))
 *           return;
 *      ...
 * }
 * \endcode
 */

static inline struct spd_str * spd_str_thread_get(struct spd_threadprivdata *ts, size_t init_len)
{
	struct spd_str *buf;

	buf = (struct spd_str *)spd_threadprivdata_get(ts,sizeof(*buf) + init_len);
	if(buf == NULL)
		return NULL;

	if(!buf->len) {
		buf->len = init_len;
		buf->used = 0;
		buf->str_tpv = ts;
	}

	return buf;
}

/*!
 * \brief Core functionality of spd_str_(set|append)_va
 *
 * The arguments to this function are the same as those described for
 * spd_str_set_va except for an addition argument, append.
 * If append is non-zero, this will append to the current string instead of
 * writing over it.
 *
 * SPD_STR_BUILD_RETRY is a legacy define.  It should probably never
 * again be used.
 *
 * A return of SPD_STR_BUILD_FAILED indicates a memory allocation error.
 *
 * A return value greater than or equal to zero indicates the number of
 * characters that have been written, not including the terminating '\0'.
 * In the append case, this only includes the number of characters appended.
 *
 * \note This function should never need to be called directly.  It should
 *       through calling one of the other functions or macros defined in this
 *       file.
 */
int __attribute__((format(printf, 4, 0))) __spd_str_helper(struct spd_str **buf, ssize_t max_len,
							   int append, const char *fmt, va_list ap);
char *__spd_str_helper2(struct spd_str **buf, ssize_t max_len,
	const char *src, size_t maxsrc, int append, int escapecommas);

/*!
 * \brief Set a dynamic string from a va_list
 *
 * \param buf This is the address of a pointer to a struct ast_str.
 *	If it is retrieved using spd_str_thread_get, the
	struct spd_threadstorage pointer will need to
 *      be updated in the case that the buffer has to be reallocated to
 *      accommodate a longer string than what it currently has space for.
 * \param max_len This is the maximum length to allow the string buffer to grow
 *      to.  If this is set to 0, then there is no maximum length.
 * \param fmt This is the format string (printf style)
 * \param ap This is the va_list
 *
 * \return The return value of this function is the same as that of the printf
 *         family of functions.
 *
 * Example usage (the first part is only for thread-local storage)
 * \code
 * SPD_THREADSTORAGE(my_str, my_str_init);
 * #define MY_STR_INIT_SIZE   128
 * ...
 * void my_func(const char *fmt, ...)
 * {
 *      struct spd_str *buf;
 *      va_list ap;
 *
 *      if (!(buf = spd_str_thread_get(&my_str, MY_STR_INIT_SIZE)))
 *           return;
 *      ...
 *      va_start(fmt, ap);
 *      spd_str_set_va(&buf, 0, fmt, ap);
 *      va_end(ap);
 * 
 *      printf("This is the string we just built: %s\n", buf->str);
 *      ...
 * }
 * \endcode
 */
static inline int __attribute__((format(printf, 3, 0))) spd_str_set_va(struct spd_str **buf, ssize_t max_len, const char *fmt, va_list ap)
{
	return __spd_str_helper(buf, max_len, 0, fmt, ap);
}

/*!
 * \brief Set a dynamic string using variable arguments
 *
 * \param buf This is the address of a pointer to a struct ast_str which should
 *      have been retrieved using spd_str_thread_get.  It will need to
 *      be updated in the case that the buffer has to be reallocated to
 *      accomodate a longer string than what it currently has space for.
 * \param max_len This is the maximum length to allow the string buffer to grow
 *      to.  If this is set to 0, then there is no maximum length.
 *	If set to -1, we are bound to the current maximum length.
 * \param fmt This is the format string (printf style)
 *
 * \return The return value of this function is the same as that of the printf
 *         family of functions.
 *
 * All the rest is the same as spd_str_set_va()
 */

/*!
 * \brief Append to a dynamic string using a va_list
 *
 * Same as spd_str_set_va(), but append to the current content.
 */
static inline int __attribute__((format(printf, 3, 0))) spd_str_append_va(struct spd_str **buf, ssize_t max_len, const char *fmt, va_list ap)
{
	return __spd_str_helper(buf, max_len, 1, fmt, ap);
}

/*!
 * \brief Set a dynamic string using variable arguments
 *
 * \param buf This is the address of a pointer to a struct ast_str which should
 *      have been retrieved using spd_str_thread_get.  It will need to
 *      be updated in the case that the buffer has to be reallocated to
 *      accomodate a longer string than what it currently has space for.
 * \param max_len This is the maximum length to allow the string buffer to grow
 *      to.  If this is set to 0, then there is no maximum length.
 *	If set to -1, we are bound to the current maximum length.
 * \param fmt This is the format string (printf style)
 *
 * \return The return value of this function is the same as that of the printf
 *         family of functions.
 *
 * All the rest is the same as spd_str_set_va()
 */

static inline int __attribute__((format(printf, 3, 4))) spd_str_set(
	struct spd_str **buf, ssize_t max_len, const char *fmt, ...)
{
	int res;
	va_list ap;

	va_start(ap, fmt);
	res = spd_str_set_va(buf, max_len, fmt, ap);
	va_end(ap);

	return res;
}

/*!
 * \brief Append to a thread local dynamic string
 *
 * The arguments, return values, and usage of this function are the same as
 * spd_str_set(), but the new data is appended to the current value.
 */
static inline int __attribute__((format(printf, 3, 4))) spd_str_append(
	struct spd_str **buf, ssize_t max_len, const char *fmt, ...)
{
	int res;
	va_list ap;

	va_start(ap, fmt);
	res = spd_str_append_va(buf, max_len, fmt, ap);
	va_end(ap);
	
	return res;
}

/*!\brief Append a non-NULL terminated substring to the end of a dynamic string. */
static inline char *spd_str_append_substr(struct spd_str **buf, ssize_t maxlen, const char *src, size_t maxsrc)
{
	return __spd_str_helper2(buf, maxlen, src, maxsrc, 1, 0);
}


/*!\brief Set a dynamic string to a non-NULL terminated substring, with escaping of commas. */
static inline char *spd_str_set_escapecommas(struct spd_str **buf, ssize_t maxlen, const char *src, size_t maxsrc)
{
	return __spd_str_helper2(buf, maxlen, src, maxsrc, 0, 1);
}

/*!\brief Append a non-NULL terminated substring to the end of a dynamic string, with escaping of commas. */
static inline char *spd_str_append_escapecommas(struct spd_str **buf, ssize_t maxlen, const char *src, size_t maxsrc)
{
	return __spd_str_helper2(buf, maxlen, src, maxsrc, 1, 1);
}


#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
