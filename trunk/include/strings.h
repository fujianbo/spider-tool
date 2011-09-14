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

#ifndef _SPIDER_STRINGS_H
#define _SPIDER_STRINGS_H

#include <ctype.h>
#include "utils.h"

/*! \brief return whether string lengh is zero
  *
  */
static inline int spd_strlen_zero(const char *s)
{
		return (!s || (*s == '\0'));
}

/*! \brief returns the equivalent of logic or for strings:
  * first one if not empty, otherwise second one.
  */
#define F_OR(a, b)	(!spd_strlen_zero(a) ? (a) : (b))

/*
 * \brief A wrapper for strdup()
 * spd_strdup() is a wrapper function for strdup that will generte a log msg in case fails
 * spd_strdup(), diff strdup(), can safely accept a NULL argument, if a NULL arg is passed
 * spd_strdup() will return NULL whthout generating any kind of log msg.
 */
#define spd_strdup(str)    \
    _spd_strdup((str), __FILE__, __LINE__, __PRETTY_FUNCTION__)

static inline void *_spd_strdup(const char *str, const char *file, int lineno, const char 
    *func)
{
    char *newstr = NULL;

    if(str) {
        if(!(newstr = strdup(str)))
            SPD_XXLLOC_FAIL_MSG;
    }

    return newstr;
}

/*!
 * \brief A wrapper for asprintf()
 * spd_asprintf is a wrapper for asprintf()m that will generate a log in case fail.
 */
 #define spd_asprintf(ret, fmt, ...)              \
    _spd_asprintf((ret), __FILE__, __LINE__,__PRETTY_FUNCTION__, fmt, __VA_ARGS__)

static inline __attribute__((format(printf, 5, 6))) int _spd_asprintf(char **ret, const 
    char *file, int lineno, const char *func, const char *fmt, ...)
{
    int res;
    va_list ap;

    va_start(ap, fmt);
    if((res = vasprintf(ret, fmt, ap)) == -1)
        SPD_XXLLOC_FAIL_MSG;
    va_end(ap);

    return res;
}

/*!
 * \brief A wrapper for asprintf()
 * spd_asprintf is a wrapper for asprintf()m that will generate a log in case fail.
 */
 #define spd_vasprintf(ret, fmt, ...)              \
    _spd_vasprintf((buf), __FILE__, __LINE__,__PRETTY_FUNCTION__, fmt, __VA_ARGS__)

static inline __attribute__((format(printf, 5, 6))) int _spd_vasprintf(char **buf, const 
    char *file, int lineno, const char *func, const char *fmt, ...)
{
    int res;
    va_list ap;

    va_start(ap, fmt);
    if((res = vasprintf(buf, fmt, ap)) == -1)
        SPD_XXLLOC_FAIL_MSG;
    va_end(ap);

    return res;
}
/*! \brief determine whether a string is a digit string 
  */
int spd_is_digitstring(const char *s);

/*!
 * \brief return a pointer to the first non whitespace char in a string
 */
char *spd_skip_blanks(const char *s);

/*!
  \brief Gets a pointer to first whitespace character in a string.
  \param spd_skip_noblanks function being used
  \param str the input string
  \return a pointer to the first whitespace character
 */
char * spd_skip_nonblanks(char *s);
/*!
 * \brief Trims trailing whitespace characters from a string.
 * \param spd_trim_blanks function being used
 * \param str the input string
 * \return a pointer to the modified string
 */
char * spd_trim_blanks(char *s);

/*!
 * \brief Strip head && tail whitespace from a string.
 * \param s The string to be stripped (will be modified).
 * \return The stripped string.

 * This functions strips all head and tail whitespace
 * characters from the input string, and returns a pointer to
 * the resulting string. The string is modified in place.
 */
char *spd_strip(char *s);

/*!
  \brief Strip leading/trailing whitespace and quotes from a string.
  \param s The string to be stripped (will be modified).
  \param beg_quotes The list of possible beginning quote characters.
  \param end_quotes The list of matching ending quote characters.
  \return The stripped string.

  This functions strips all leading and trailing whitespace
  characters from the input string, and returns a pointer to
  the resulting string. The string is modified in place.

  It can also remove beginning and ending quote (or quote-like)
  characters, in matching pairs. If the first character of the
  string matches any character in beg_quotes, and the last
  character of the string is the matching character in
  end_quotes, then they are removed from the string.

  Examples:
  \code
  spd_strip_quoted(buf, "\"", "\"");
  spd_strip_quoted(buf, "'", "'");
  spd_strip_quoted(buf, "[{", "})");
  \endcode
 */
char *spd_strip_quoted(char *s, const char *first_quote, const char *last_quote);

/*!
  \brief Strip backslash for "escaped" semicolons.
  \brief s The string to be stripped (will be modified).
  \return The stripped string.
 */
char *spd_unescape_semicolon(char *s);

/*!
  \brief Size-limited null-terminating string copy.
  \param spd_copy_string function being used
  \param dst The destination buffer.
  \param src The source string
  \param size The size of the destination buffer
  \return Nothing.

  This is similar to \a strncpy, with two important differences:
    - the destination buffer will \b always be null-terminated
    - the destination buffer is not filled with zeros past the copied string length
  These differences make it slightly more efficient, and safer to use since it will
  not leave the destination buffer unterminated. There is no need to pass an artificially
  reduced buffer size to this function (unlike \a strncpy), and the buffer does not need
  to be initialized to zeroes prior to calling this function.
*/
void spd_copy_string(char *dst, const char *src, size_t size);

/*!
  * \brief   update the valur of src with newval
  */
void spd_strupdate(char **src, const char *newval);

/*
  *  \brief format to a string 
  */
int spd_snprintf(char *buf, int size, const char *fmt,...);

/*
  \brief Join an array of strings into a single string.
  \param s the resulting string buffer
  \param len the length of the result buffer, s
  \param w an array of strings to join

  This function will join all of the strings in the array 'w' into a single
  string.  It will also place a space in the result buffer in between each
  string from 'w'.
*/
void spd_str_join(char *s, size_t len, char *const w[]);

#endif
