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

/*! 
  * \brief  Unite Test Framework
  *
  */

#ifndef _TEST_ENGINE_H
#define _TEST_ENGINE_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define SPD_TEST_FRAMEWORK

#ifdef SPD_TEST_FRAMEWORK
#define SPD_TEST_INIT(name) static enum spd_test_result name(struct spd_test_record *record, enum spd_test_cmd type, struct spd_test *test)
#define SPD_TEST_REGISTER(name) spd_test_register(name)
#define SPD_TEST_UNREGISTER(name) spd_test_unregister(name)
#define SPD_TEST_REPORT(name, category,file) spd_test_report(name, category,file)
#define SPD_TEST_RUN(name, category) spd_test_run(name, category)
#else 
#define SPD_TEST_INIT(name)
#define SPD_TEST_REGISTER(name)
#define SPD_TEST_UNREGISTER(name)
#define SPD_TEST_REPORT(name, category,file)
#define SPD_TEST_RUN(name, category)
#endif

enum spd_test_result {
	TEST_RESULT_NOT_RUN,
	TEST_RESULT_PASS,
	TEST_RESULT_FAILED,
};

enum spd_test_cmd {
	SPD_TEST_CMD_INIT,
	SPD_TEST_CMD_RUN,
};

struct spd_test_record {
	const char *name; /* unique name of this  test */
	const char *category; /*  test in this category can be run */
	const char *description; /*  a description of test  */
};

struct spd_test;

typedef enum spd_test_result (spd_test_cb_t)(struct spd_test_record *record, enum spd_test_cmd type, struct spd_test* test);

/* register a test record */
int spd_test_register(spd_test_cb_t *cb);

/* unresigster a test record  */
int spd_test_unregister(spd_test_cb_t *cb);


/* 
  * run test framework  in three mode :
  * name : run given name test case, may be NULL 
  * category : run a class of test of given category , may be NULL
  * if both name and category is NULL , will run all test case.
  *
  */
int spd_test_run(const char *name, const char *category);

/*!
  *\brief report test result in three mode ,by name, by category or all. may write result to textfile.
  */
int spd_test_report(const char *name, const char *category, const char *textfile);



int __spd_test_update_state(const char *file, const char *func, int line , struct spd_test *test, 
		const char *fmt, ...);

#define spd_test_update_state(test, fmt,...)  __spd_test_update_state(__FILE__, __PRETTY_FUNCTION__, __LINE__, (test), (fmt), ## __VA_ARGS__)

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif