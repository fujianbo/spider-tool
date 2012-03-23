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

#include "test_engine.h"
#include "logger.h"
#include "str.h"
#include "linkedlist.h"
#include "times.h"


 
/* stand for a  single test record */
struct spd_test {
	struct spd_test_record info;   /* store test info */
	enum spd_test_result result;   /* test result */
	struct spd_str *status_str;    /* test status in string */
	unsigned int howlong;  		   /* time this test cost */
	spd_test_cb_t *callback;       /* real func perform test work */
	SPD_LIST_ENTRY(spd_test)  list;
};

struct test_resultstr{
	enum spd_test_result result;
	const char* state;
} ;

static const struct test_resultstr test_result2str [] = {
		{TEST_RESULT_NOT_RUN, "NOT RUN"},
		{TEST_RESULT_PASS, "PASS"},
		{TEST_RESULT_FAILED, "FAILED"},
};

/* time mode */
enum test_type {
	TEST_NAME = 0,
	TEST_CATEGORY,
	TEST_ALL,
};

/*! global structure containing both total and last test execution results */
static struct spd_test_execute_results {
	unsigned int total_tests;  /*!< total number of tests, regardless if they have been executed or not */
	unsigned int total_passed; /*!< total number of executed tests passed */
	unsigned int total_failed; /*!< total number of executed tests failed */
	unsigned int total_time;   /*!< total time of all executed tests */
	unsigned int last_passed;  /*!< number of passed tests during last execution */
	unsigned int last_failed;  /*!< number of failed tests during last execution */
	unsigned int last_time;    /*!< total time of the last test execution */
} last_results;

static SPD_LIST_HEAD_STATIC(spd_test_list, spd_test);
static struct spd_test* spd_test_create(spd_test_cb_t*callback);
static struct spd_test* spd_test_free(struct spd_test *test);
static int spd_test_insert(struct spd_test *test);
static struct spd_test* spd_test_remove(spd_test_cb_t* cb);
static int spd_test_category_cmp(const char *cat1, const char *cat2);

int __spd_test_update_state(const char * file, const char * func, int line, struct spd_test * test, const char * fmt,...)
{
	struct spd_str *buf = NULL;
	va_list ap;

	if(!(buf = spd_str_create(128))) {
		return -1;
	}

	va_start(ap, fmt);
	spd_str_set_va(&buf, 0, fmt, ap);
	va_end(ap);

	spd_str_append(&test->status_str,0, "[%s:%s:%d]: %s",
			file, func, line,spd_str_buffer(buf));

	spd_safe_free(buf);

	return 0;
}

int spd_test_register(spd_test_cb_t *cb)
{
	struct spd_test *test;

	if(!cb) {
		spd_log(LOG_WARNING, "No callback be specified\n");
		return -1;
	}

	if(!(test = spd_test_create(cb))) {
		spd_log(LOG_WARNING, "failed to create test case\n");
		return -1;
	}

	if(spd_test_insert(test)) {
		spd_log(LOG_WARNING, "failed to insert test \n");
		spd_test_free(test);
		return -1;
	}

	return -1;
}

int spd_test_unregister(spd_test_cb_t *cb)
{
	struct spd_test *test;
	
	if(!cb)  {
		spd_log(LOG_WARNING, "No callback be specified\n");
		return -1;
	}

	if(!(test = spd_test_remove(cb))) {
		return -1;
	}

	spd_test_free(test);

	return 0;
}

static struct spd_test *spd_test_free(struct spd_test* test)
{
	if(!test) {
		return NULL;
	}

	spd_safe_free(test->status_str);
	spd_safe_free(test);

	return NULL;
}

static struct spd_test *spd_test_create(spd_test_cb_t * callback)
{
	struct spd_test *test = NULL;

	if(!callback || !(test = spd_calloc(1, sizeof(struct spd_test)))) {
		return NULL;
	}

	test->callback = callback;
	test->callback(&test->info, SPD_TEST_CMD_INIT, test);
	
	if(spd_strlen_zero(test->info.category)) {
		spd_log(LOG_WARNING, "No category set, cannot register. \n");
		return spd_test_free(test);
	}

	if(spd_strlen_zero(test->info.name)) {
		spd_log(LOG_WARNING, "No name set , cannot register. \n");
		return spd_test_free(test);
	}

	if(test->info.category[0] != '/' || test->info.category[strlen(test->info.category) - 1] != '/') {
		spd_log(LOG_WARNING, "category %s must start with / and end whith /.\n", test->info.category);
	}

	if(spd_strlen_zero(test->info.description)) {
		spd_log(LOG_WARNING, "NO desc set , cannot register.\n");
		return spd_test_free(test);
	}

	if(!(test->status_str = spd_str_create(128))) {
		return spd_test_free(test);
	}

	return test;
	
}

static int spd_test_insert(struct spd_test *test)
{	
	SPD_LIST_LOCK(&spd_test_list);
	SPD_LIST_INSERT_SORTALPHA(&spd_test_list, test,list,info.category);
	SPD_LIST_UNLOCK(&spd_test_list);

	return 0;
}

static struct spd_test* spd_test_remove(spd_test_cb_t *cb)
{
	struct spd_test *cur = NULL;

	SPD_LIST_LOCK(&spd_test_list);
	SPD_LIST_TRAVERSE_SAFE_BEGIN(&spd_test_list, cur, list) {
		if(cur->callback == cb) {
			SPD_LIST_REMOVE_CURRENT(&spd_test_list, list);
			break;
		}
	}
	SPD_LIST_TRAVERSE_SAFE_END;
	SPD_LIST_UNLOCK(&spd_test_list);

	return cur;
}

static int spd_test_category_cmp(const char *cat1, const char *cat2)
{
	int len1 = 0;
	int len2 = 0;

	if (!cat1 || !cat2) {
		return -1;
	}

	len1 = strlen(cat1);
	len2 = strlen(cat2);

	if (len2 > len1) {
		return -1;
	}

	return strncmp(cat1, cat2, len2) ? 1 : 0;
}

static int run_one(struct spd_test *test)
{
	
	struct timeval start =  spd_tvnow();
	
	spd_str_reset(test->status_str);
	
	test->result = test->callback(&test->info, SPD_TEST_CMD_RUN, test);
	
	test->howlong = spd_tvdiff_ms(spd_tvnow(), start);

	return 0;
}

int spd_test_run(const char * name, const char * category)
{
	enum test_type mode = TEST_ALL;
	struct spd_test *test = NULL;
	int ret = 0;
	int run = 0;
	
	if(!spd_strlen_zero(category)) {
		if(spd_strlen_zero(name)) {
			mode = TEST_NAME;
		} else {
			mode = TEST_CATEGORY;
		}
	}

	SPD_LIST_LOCK(&spd_test_list);

	memset(&last_results, 0, sizeof(last_results));
	SPD_LIST_TRAVERSE(&spd_test_list, test, list) {
		run = 0;
		switch(mode) {
		case TEST_CATEGORY:
			if(!spd_test_category_cmp(test->info.category, category)) {
				run = 1;
			}
			break;
		case TEST_NAME:
			if(!spd_test_category_cmp(test->info.category, category) && !strcmp(test->info.name, name)) {
				run = 1;
			}
			break;
		case TEST_ALL:
			run = 1;
			break;
		}

		if(run) {
			spd_log(LOG_DEBUG, "START Unite Test  %s - %s \n",test->info.category, test->info.name);

			run_one(test);

			last_results.last_time += test->howlong;
			if(test->result == TEST_RESULT_PASS) {
				last_results.last_passed++;
			} else if(test->result = TEST_RESULT_FAILED) {
				last_results.last_failed++;
			}
		}

		last_results.total_time += test->howlong;
		last_results.total_tests++;
		if(test->result != TEST_RESULT_NOT_RUN) {
			if(test->result == TEST_RESULT_PASS) {
				last_results.total_passed++;
			} else {
				last_results.total_failed++;
			}
		}
	}
	
	ret = last_results.total_passed + last_results.total_failed;

	if(!(last_results.last_passed + last_results.last_failed)) {
		spd_log(LOG_WARNING, "No Test Found.\n ");
		return ret;
	} else {
	
		spd_log(LOG_NOTICE, "Number of Tests:            %d\n", (last_results.last_passed + last_results.last_failed));
		spd_log(LOG_NOTICE, "Number of Tests Executed:   %d\n",last_results.total_tests);
		spd_log(LOG_NOTICE, "Passed Tests:               %d\n", last_results.last_passed);
		spd_log(LOG_NOTICE, "Failed Tests:               %d\n",last_results.last_failed);
		spd_log(LOG_NOTICE, "Total Execution Time:       %d\n", last_results.total_time);
	}
	
	SPD_LIST_UNLOCK(&spd_test_list);
	
	return ret;
}

static void test_report_one(FILE *txtfile, const struct spd_test *test)
{
	if(!txtfile || !test) {
		return;
	}

	fprintf(txtfile, "\nName:           %s\n", test->info.name);
	fprintf(txtfile, "Category:         %s\n", test->info.category);
	fprintf(txtfile, "Description:      %s\n", test->info.description);
	fprintf(txtfile, "Result:           %s\n", test_result2str[test->result].state);

	if(test->result != TEST_RESULT_NOT_RUN) {
		fprintf(txtfile, "Time:          %d\n",test->howlong);
	}

	if(test->result == TEST_RESULT_FAILED) {
		fprintf(txtfile, "Error Description:  %s\n", spd_str_buffer(test->status_str));
	}
}

int spd_test_report(const char * name, const char * category, const char * textfile)
{
	enum test_type mode = TEST_ALL;
	FILE *ftext;
	struct spd_test *test = NULL;

	if(!spd_strlen_zero(category)) {
		if(spd_strlen_zero(name)) {
			mode = TEST_NAME;
		} else {
			mode = TEST_CATEGORY;
		}
	}

	if(!spd_strlen_zero(textfile)) {
		if(!(ftext = fopen(textfile, "w"))) {
			spd_log(LOG_ERROR, "cannot open textfile : %s\n", textfile);
			return -1;
		}
	}

	SPD_LIST_LOCK(&spd_test_list);

	if(ftext) {
		fprintf(ftext,"Number of Tests:            %d\n", last_results.total_tests);
		fprintf(ftext,"Number of Tests Executed:   %d\n",(last_results.total_passed + last_results.total_failed));
		fprintf(ftext,"Passed Tests:               %d\n", last_results.total_passed);
		fprintf(ftext,"Failed Tests:               %d\n", last_results.total_failed);
		fprintf(ftext,"Total Execution Time:       %d\n", last_results.total_time);
	}

	SPD_LIST_TRAVERSE(&spd_test_list, test, list) {
		
		switch(mode) {
		case TEST_CATEGORY:
			if(!spd_test_category_cmp(test->info.category, category)) {
				test_report_one(ftext, test);
			}
			break;
		case TEST_NAME:
			if(!spd_test_category_cmp(test->info.category, category) && !strcmp(test->info.name, name)) {
				test_report_one(ftext, test);
			}
			break;
		case TEST_ALL:
			test_report_one(ftext, test);
			break;
		}
	}
	
	SPD_LIST_UNLOCK(&spd_test_list);

	if(ftext)
		fclose(ftext);

	return 0;
}
