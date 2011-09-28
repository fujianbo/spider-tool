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

 /*! \file
 * \brief Time-related functions and macros
 */

#ifndef _SPIDER_TIME_H
#define _SPIDER_TIME_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <sys/time.h>


//typedef long long int64_t;

/*!
 * \brief Computes the difference (in seconds) between two \c struct \c timeval instances.
 * \param end the end of the time period
 * \param start the beginning of the time period
 * \return the difference in seconds
 */
static inline int64_t spd_tvdiff_sec(struct timeval end, struct timeval start)
{
	int64_t result = end.tv_sec - start.tv_sec;
	if (result > 0 && end.tv_usec < start.tv_usec)
		result--;
	else if (result < 0 && end.tv_usec > start.tv_usec)
		result++;

	return result;
}

/*
  * get current time , replace gettimeofday
  */
static inline timval spd_tvnow()
{
	struct timeval t;
	gettimeofday(&t, NULL);

	return t;
}

/*!
 * \brief Computes the difference (in microseconds) between two \c struct \c timeval instances.
 * \param end the end of the time period
 * \param start the beginning of the time period
 * \return the difference in microseconds
 */
static inline int64_t spd_tvdiff_us(struct timeval end, struct timeval start)
{
	return (end.tv_sec - start.tv_sec) * (int64_t) 1000000 +
		end.tv_usec - start.tv_usec;
}

/*!
 * \brief Computes the difference (in milliseconds) between two \c struct \c timeval instances.
 * \param end end of the time period
 * \param start beginning of the time period
 * \return the difference in milliseconds
 */
static inline int64_t spd_tvdiff_ms(struct timeval end, struct timeval start)
{
	/* the offset by 1,000,000 below is intentional...
	   it avoids differences in the way that division
	   is handled for positive and negative numbers, by ensuring
	   that the divisor is always positive
	*/
	return  ((end.tv_sec - start.tv_sec) * 1000) +
		(((1000000 + end.tv_usec - start.tv_usec) / 1000) - 1000);
}

/*!
 * \brief Returns true if the argument is 0,0
 */
static inline int spd_tvzero(const struct timeval t)
{
	return (t.tv_sec == 0 && t.tv_usec == 0);
}

/*!
 * \brief Compres two \c struct \c timeval instances returning
 * -1, 0, 1 if the first arg is smaller, equal or greater to the second.
 */
static inline int spd_tvcmp(struct timeval _a, struct timeval _b)
{
	if (_a.tv_sec < _b.tv_sec)
		return -1;
	if (_a.tv_sec > _b.tv_sec)
		return 1;
	/* now seconds are equal */
	if (_a.tv_usec < _b.tv_usec)
		return -1;
	if (_a.tv_usec > _b.tv_usec)
		return 1;
	return 0;
}

/*!
 * \brief Returns true if the two \c struct \c timeval arguments are equal.
 */
static inline int spd_tveq(struct timeval _a, struct timeval _b)
{
	return (_a.tv_sec == _b.tv_sec && _a.tv_usec == _b.tv_usec);
}

/*!
 * \brief Returns current timeval. Meant to replace calls to gettimeofday().
 */
static inline struct timeval ast_tvnow(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t;
}

/*!
 * \brief Returns the sum of two timevals a + b
 */
struct timeval spd_tvadd(struct timeval a, struct timeval b);

/*!
 * \brief Returns the difference of two timevals a - b
 */
struct timeval spd_tvsub(struct timeval a, struct timeval b);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif