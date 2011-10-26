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

/* commen code used to set thread specific data */

 #ifndef _SPD_THREADPRIVDATA_H
 #define _SPD_THREADPRIVDATA_H


#include "lock.h"
#include "utils.h"
#include "strings.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/*!
  * \brief data for a thread locally stored variable
  */
struct spd_threadprivdata {
    pthread_once_t once;
    pthread_key_t key;
    void(*init_key)(void);
    int (*init_custom)(void *);
} spd_threadprivdata;


enum {
    /*! An error has occured and the contents of the dynamic string
	 *  are undefined */
    SPD_DNMCSTR_BUILD_FAILEDD = -1,
     /*! The buffer size for the dynamic string had to be increased, and
	 *  spd_dynamic_str_thread_build_va() needs to be called again after
	 *  a va_end() and va_start().
	 */
    SPD_DNMCSTR_BUILD_RETRY = -2,
 };

/*!
 * \brief Define a thread storage variable
 *
 * \param name The name of the thread storage object
 *
 * This macro would be used to declare an instance of thread storage in a file.
 *
 * Example usage:
 * \code
 * SPD_THREADSTORAGE(my_buf);
 * \endcode
 */
#define SPD_THREADPRIVDATA(name)   \
    SPD_THREADPRIVDATA_CUSTOM_SCOP(name, NULL, free, static)
#define SPD_THREADPRIVDATA_PUBLIC(name)  \
    SPD_THREADPRIVDATA_CUSTOM_SCOP(name, NULL, free,)
#define SPD_THREADORIVDATA_EXTERNAL(name)  \
    extern struct spd_threadprivdata

 /*!
  * \brief Define a thread storage variable, with custom initialization and cleanup
  *
  * \param name The name of the thread storage object
  * \param init This is a custom function that will be called after each thread specific
  *           object is allocated, with the allocated block of memory passed
  *           as the argument.
  * \param cleanup This is a custom function that will be called instead of spd_free
  *              when the thread goes away.  Note that if this is used, it *MUST*
  *              call free on the allocated memory.
  *
  * Example usage:
  * \code
  * SPD_THREADPRIVDATA_CUSTOM(my_buf, my_init, my_cleanup);
  * \endcode
  */
#define SPD_THREADSTORAGE_CUSTOM(a,b,c)	SPD_THREADPRIVDATA_CUSTOM_SCOP(a,b,c,static)

#define SPD_PTHREAD_ONCE_INIT  PTHREAD_ONCE_INIT

#define SPD_THREADPRIVDATA_CUSTOM_SCOP(name, k_init, k_cleanup, scop)  \
    static void __init_##name(void);                  \
    scop struct spd_threadprivdata name = {           \
    .once = SPD_PTHREAD_ONCE_INIT,                    \
    .init_key = __init_##name,                        \
    .init_custom = k_init,                            \
};                                                    \
static void __init_##name(void)                       \
{                                                     \
    pthread_key_create(&(name).key, k_cleanup);       \
}                                                     

 /*!
  * \brief Retrieve thread storage
  *
  * \param ts This is a pointer to the thread storage structure declared by using
  *      the SPD_THREADSTORAGE macro.  If declared with 
  *      SPD_THREADSTORAGE(my_buf), then this argument would be (&my_buf).
  * \param init_size This is the amount of space to be allocated the first time
  *      this thread requests its data. Thus, this should be the size that the
  *      code accessing this thread storage is assuming the size to be.
  *
  * \return This function will return the thread local storage associated with
  *         the thread storage management variable passed as the first argument.
  *         The result will be NULL in the case of a memory allocation error.
  *
  * Example usage:
  * \code
  * SPD_THREADSTORAGE(my_buf);
  * #define MY_BUF_SIZE   128
  * ...
  * void my_func(const char *fmt, ...)
  * {
  *      void *buf;
  *
  *      if (!(buf = spd_threadstorage_get(&my_buf, MY_BUF_SIZE)))
  *           return;
  *      ...
  * }
  * \endcode
  */
void *spd_threadprivdata_get(struct spd_threadprivdata *ts, size_t init_size);


/* thread dynamic string define */

/*!
 * \brief spider thread private data dynamic string 
 */
struct spd_dynamic_str {
    size_t len;   /* string length*/
    char str[0];  /* real store */
} spd_dynamic_str;

/*!
 * \brief Create a dynamic length string
 *
 * \arg init_len This is the initial length of the string buffer
 *
 * \return This function returns a pointer to the dynamic string length.  The
 *         result will be NULL in the case of a memory allocation error.
 *
 * /note The result of this function is dynamically allocated memory, and must
 *       be free()'d after it is no longer needed.
 */
struct spd_dynamic_str * spd_dynamic_str_create(size_t len);

/*!
  * \brief Retrieve a thread locally stored dynamic string
  *
  * \arg ts This is a pointer to the thread storage structure declared by using
  *      the SPD_THREADPRIVDATA macro.  If declared with 
  *      SPD_THREADPRIVDATA(my_buf, my_buf_init), then this argument would be 
  *      (&my_buf).
  * \arg init_len This is the initial length of the thread's dynamic string. The
  *      current length may be bigger if previous operations in this thread have
  *      caused it to increase.
  *
  * \return This function will return the thread locally storaged dynamic string
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
  *      struct spd_dynamic_str *buf;
  *
  *      if (!(buf = spd_dynamic_str_thread_get(&my_str, MY_STR_INIT_SIZE)))
  *           return;
  *      ...
  * }
  * \endcode
  */
struct spd_dynamic_str *spd_dynamic_str_get(struct spd_threadprivdata *tpd, size_t 
    len);


/*!
  * \brief Set a thread locally stored dynamic string from a va_list
  *
  * \arg buf This is the address of a pointer to an spd_dynamic_str which should
  *      have been retrieved using spd_dynamic_str_thread_get.  It will need to
  *      be updated in the case that the buffer has to be reallocated to
  *      accomodate a longer string than what it currently has space for.
  * \arg max_len This is the maximum length to allow the string buffer to grow
  *      to.  If this is set to 0, then there is no maximum length.
  * \arg ts This is a pointer to the thread storage structure declared by using
  *      the SPD_THREADPRIVDATA macro.  If declared with 
  *      SPD_THREADPRIVDATA(my_buf, my_buf_init), then this argument would be 
  *      (&my_buf).
  * \arg fmt This is the format string (printf style)
  * \arg ap This is the va_list
  *
  * \return The return value of this function is the same as that of the printf
  *         family of functions.
  *
  * Example usage:
  * \code
  * SPD_THREADPRIVDATA(my_str, my_str_init);
  * #define MY_STR_INIT_SIZE   128
  * ...
  * void my_func(const char *fmt, ...)
  * {
  *      struct spd_dynamic_str *buf;
  *      va_list ap;
  *
  *      if (!(buf = spd_thread_dynamic_str_get(&my_str, MY_STR_INIT_SIZE)))
  *           return;
  *      ...
  *      va_start(fmt, ap);
  *      spd_thread_dynamic_str_set_helper(&buf, 0, &my_str, fmt, ap);
  *      va_end(ap);
  * 
  *      printf("This is the string we just built: %s\n", buf->str);
  *      ...
  * }
  * \endcode
  */
#define spd_thread_dynamic_str_set_helper(buf, len, tpd, fmt, ap)   \
({                                                   \
    int __res;                                    \
    while((__res = spd_thread_dynamic_str_build_helper(buf, len, tpd, 0, fmt, ap)) ==\
        SPD_DNMCSTR_BUILD_RETRY)   {     \
        va_end(ap);                        \
        va_start(ap, fmt);                  \
    }                                        \
    (__res);                                  \
})

/*!
 * \brief Append to a thread local dynamic string using a va_list
 *
 * The arguments, return values, and usage of this are the same as those for
 * spd_thread_dynamic_str_set_helper().  However, instead of setting a new value
 * for the string, this will append to the current value.
 */
#define spd_thread_dynamic_str_append_helper(buf, max_len, ts, fmt, ap)              \
({                                                                       \
    int __res;                                                       \
    while ((__res = spd_thread_dynamic_str_build_helper(buf, max_len,    \
        ts, 1, fmt, ap)) == SPD_DNMCSTR_BUILD_RETRY) {            \
        va_end(ap);                                              \
        va_start(ap, fmt);                                       \
        }                                                                \
        (__res);                                                         \
})

/*!
 * \brief Core functionality of spd_thread_dynamic_str_(set|append)_helper
 *
 * The arguments to this function are the same as those described for
 * spd_dynamic_str_thread_set_va except for an addition argument, append.
 * If append is non-zero, this will append to the current string instead of
 * writing over it.
 */
int spd_thread_dynamic_str_build_helper(struct spd_dynamic_str **buf, size_t len,
    struct spd_threadprivdata *tpd, int append, const char *fmt, va_list ap);

/* \brief Set a thread locally stored dynamic string using variable arguments
 *
 * \arg buf This is the address of a pointer to an spd_dynamic_str which should
 *      have been retrieved using spd_dynamic_str_thread_get.  It will need to
 *      be updated in the case that the buffer has to be reallocated to
 *      accomodate a longer string than what it currently has space for.
 * \arg max_len This is the maximum length to allow the string buffer to grow
 *      to.  If this is set to 0, then there is no maximum length.
 * \arg ts This is a pointer to the thread storage structure declared by using
 *      the SPD_THREADSTORAGE macro.  If declared with 
 *
 *      SPD_THREADSTORAGE(my_buf, my_buf_init), then this argument would be 
 *      (&my_buf).
 * \arg fmt This is the format string (printf style)
 *
 * \return The return value of this function is the same as that of the printf
 *         family of functions.
 *
 * Example usage:
 *
 * \code
 * SPD_THREADSTORAGE(my_str, my_str_init);
 * #define MY_STR_INIT_SIZE   128
 * ...
 * void my_func(int arg1, int arg2)
 * {
 *      struct spd_dynamic_str *buf;
 *      va_list ap;
 *
 *      if (!(buf = spd_dynamic_str_thread_get(&my_str, MY_STR_INIT_SIZE)))
 *           return;
 *      ...
 *      spd_dynamic_str_thread_set(&buf, 0, &my_str, "arg1: %d  arg2: %d\n",
 *           arg1, arg2);
 * 
 *      printf("This is the string we just built: %s\n", buf->str);
 *      ...
 * }
 *\endcode
 */

int  spd_dynamic_str_set(
    struct spd_dynamic_str **buf, size_t len, 
    struct spd_threadprivdata *tpd, const char *fmt, ...);

/*!
 * \brief Append to a thread local dynamic string
 *
 * The arguments, return values, and usage of this function are the same as
 * spd_thread_dynamic_str_set().  However, instead of setting a new value for
 * the string, this function appends to the current value.
 */
 int  spd_dynamic_str_append(
    struct spd_dynamic_str **buf, size_t len, 
    struct spd_threadprivdata *tpd, const char *fmt, ...);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif