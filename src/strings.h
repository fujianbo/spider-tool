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

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static char HEX[] = "0123456789abcdef";

/*
* From base 10 to base 16
* @param c the base 10 char to convert to base 16
* @retval The base 16 value
*/
char spd_b10tob16(char c);

/*
* From base 16 to base 10
* @param c The base 16 char to convert to base 10
* @retval The base 10 value
*/
char spd_b16tob10(char c);

/**	Duplicates the first @a n chars of @a str.
 * @param s1 The string to duplicate. 
 * @param n The number of characters to copy to the new string. 
 * @retval	null A copy of @a str. 
**/
char *spd_strndup(const char *str, int size);

/*
* Checks if @a str contains @a substring.
* @param str The master string.
* @param size The size of the master string.
* @param substring the substring.
* @retval @a true if @a str contains at least one occurence of @a substring and @a false othewise.
*/
int spd_strcontains(const char *str, int size, const char *substr);

/*! 
  *\brief return whether string lengh is zero
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

/*!
  \brief duplicate a string in memory from the stack
  \param s The string to duplicate

  This macro will duplicate the given string.  It returns a pointer to the stack
  allocatted memory for the new string.
*/
#define spd_strdupa(str)                                           \
(__extension__                                                    \
	({                                                                \
		const char *__old = (s);                                  \
		size_t __len = strlen(__old) + 1;                         \
		char *__new = __builtin_alloca(__len);                    \
		memcpy (__new, __old, __len);                             \
		__new;                                                    \
	}))
	
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
char *spd_skip_nonblanks(char *s);
/*!
 * \brief Trims trailing whitespace characters from a string.
 * \param spd_trim_blanks function being used
 * \param str the input string
 * \return a pointer to the modified string
 */
char *spd_trim_blanks(char *s);

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
  *\brief Gets the first occurrence of @a substring within @a str.
  * @param str The master string.
  * @param size The size of the master string.
  * @param substring The substring that is to be searched for within @a str.
  * @retval The index of the first ocurrence of @a substring in @a str.
  * If no occurrence of @a substring is found, then -1 is returned.
  */
int spd_stringindex(const char *str, int size, const char *substr);

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
void spd_str_join(char *s, size_t len, const char *const w[]);

/*!
 * \brief Compute a hash value on a case-insensitive string
 *
 * Uses the same hash algorithm as spd_str_hash, but converts
 * all characters to lowercase prior to computing a hash. This
 * allows for easy case-insensitive lookups in a hash table.
 */
static inline int spd_str_case_hash(const char *name)
{
	int hash = 5381;
	while(*name) {
		hash = hash * 33 ^ tolower(*name++);
	}

	return abs(hash);
}

char *spd_strtok(char *str, const char *sep, char **last)
{
    char *token;

    if (!str)           /* subsequent call */
        str = *last;    /* start where we left off */

    /* skip characters in sep (will terminate at '\0') */
    while (*str && strchr(sep, *str))
        ++str;

    if (!*str)          /* no more tokens */
        return NULL;

    token = str;

    /* skip valid token characters to terminate token and
     * prepare for the next call (will terminate at '\0) 
     */
    *last = token + 1;
    while (**last && !strchr(sep, **last))
        ++*last;

    if (**last) {
        **last = '\0';
        ++*last;
    }

    return token;
}


#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
