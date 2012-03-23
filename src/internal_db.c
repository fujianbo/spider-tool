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

 #include <sys/time.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
 #include <signal.h>
 #include <dirent.h>
 #include <sqlite3.h>


 #include "logger.h"
 #include "utils.h"


/*
  * sqlite c interface api
  * http://www.sqlite.org/c3ref/open.html
  */

SPD_MUTEX_DEFINE_STATIC(dblock);
static spd_cond_t dbcond;
static pthread_t  dbsync_thread;
static int doexit;
static sqlite3 *spd_db;
const char* spddb_dir = "/tmp/spddb";


#define SPD_DB_STATEMENT_DEFINE(stmt, sql) static sqlite3_stmt *stmt; \
	const char stmt##_sql[] = sql;

SPD_DB_STATEMENT_DEFINE(put_stmt, "INSERT OR REPLACE INTO spd_db (key, value) VALUES (?, ?)")
SPD_DB_STATEMENT_DEFINE(get_stmt, "SELECT value FROM spd_db WHERE key =?")
SPD_DB_STATEMENT_DEFINE(del_stmt, "DELETE FROM spd_db WHERE key=?")
SPD_DB_STATEMENT_DEFINE(deltree_stmt, "DELETE FROM spd_db WHERE key || '/' LIKE ? || '/' || '%'")
SPD_DB_STATEMENT_DEFINE(deltree_all_stmt, "DELETE FROM spd_db")
SPD_DB_STATEMENT_DEFINE(gettree_stmt,"SELECT key, value FROM spd_db WHERE key || '/' LIKE ? || '/' || '%' ORDER BY key")
SPD_DB_STATEMENT_DEFINE(gettree_all_stmt, "SELECT key, value FROM spd_db ORDER BY key")
SPD_DB_STATEMENT_DEFINE(create_spd_db_stmt, "CREATE TABLE IF NOT EXISTS spd_db(key VARCHAR(256), value VARCHAR(256), PRIMARY KEY(key))");

static int db_open()
{
	char *dbname;
	if(!(dbname = alloca(strlen(spddb_dir) + sizeof(".db")))) {
		spd_log(LOG_ERROR, "alloca failed\n");
		return -1;
	}	

	strcpy(dbname, spddb_dir);
	strcat(dbname, ".db");

	spd_mutex_lock(&dblock);
	if(sqlite3_open(dbname, &spd_db) != SQLITE_OK) {
		spd_log(LOG_ERROR, "failed open db '%s' %s\n", dbname, sqlite3_errmsg(spd_db));
		spd_mutex_unlock(&dblock);
		return -1;
	}
	spd_mutex_unlock(&dblock);

}

/*
  * sync db signal , must lock db first.
  */
static void do_dbsync()
{
	spd_cond_signal(&dbcond);
}

static int init_stmt(sqlite3_stmt **stmt, const char *sql, size_t len)
{
	spd_mutex_lock(&dblock);
	if(sqlite3_prepare(spd_db, sql, len, stmt, NULL) != SQLITE_OK) {
		spd_log(LOG_WARNING, "could not prepare statement '%s': %s\n", sql, sqlite3_errmsg(spd_db));
		spd_mutex_unlock(&dblock);
		return -1;
	}
	spd_mutex_unlock(&dblock);

	return 0;
}

static int db_create()
{
	int ret = 0;
	
	if(!create_spd_db_stmt) {
		init_stmt(&create_spd_db_stmt, create_spd_db_stmt_sql, sizeof(create_spd_db_stmt_sql));
	}

	spd_mutex_lock(&dblock);
	if(sqlite3_step(create_spd_db_stmt) != SQLITE_DONE) {
		spd_log(LOG_WARNING, "Couldnot create spd db table: %s\n", sqlite3_errmsg(spd_db));
		ret = -1;
	}

	sqlite3_reset(create_spd_db_stmt);
	do_dbsync();
	spd_mutex_unlock(&dblock);

	return ret;
}

/*
 * init stmt on start up for performance.
 */
static int db_init_statements()
{
	return init_stmt(&get_stmt, get_stmt_sql, sizeof(get_stmt_sql))
		|| init_stmt(&del_stmt, del_stmt_sql, sizeof(del_stmt_sql))
		|| init_stmt(&deltree_stmt, deltree_stmt_sql, sizeof(deltree_stmt_sql))
		|| init_stmt(&deltree_all_stmt, deltree_all_stmt_sql, sizeof(deltree_all_stmt_sql))
		|| init_stmt(&gettree_stmt, gettree_stmt_sql, sizeof(gettree_stmt_sql))
		|| init_stmt(&gettree_all_stmt, gettree_all_stmt_sql, sizeof(gettree_all_stmt_sql))
		|| init_stmt(&put_stmt, put_stmt_sql, sizeof(put_stmt_sql));
}



/* init db engine , open ,create and init stmt */
static int db_init()
{
	if(spd_db)
		return 0;

	if(db_open() || db_create() || db_init_statements())
		return -1;

	return 0;
}

static void spd_db_close(sqlite3 *db)
{
	sqlite3_close(db);
}

/*
  * perform clean up resource.
  */
void spddb_uninit(void)
{
	spd_log(LOG_NOTICE,"uninit db \n");
	doexit = 1;
	spd_mutex_lock(&dblock);
	do_dbsync();
	spd_mutex_unlock(&dblock);
	
	pthread_join(dbsync_thread, NULL);
	spd_mutex_lock(&dblock);
	spd_db_close(spd_db);
	spd_mutex_unlock(&dblock);

	spd_mutex_destroy(&dblock);
	spd_cond_destroy(&dbcond);
}

int spd_db_get(const char * table, const char * key, char * buf, int len)
{
	const unsigned char *result;
	char fullkey[256];
	size_t fullkey_len;
	int res = 0;

	if(strlen(table) + strlen(key) + 2 > sizeof(fullkey) -1) {
		spd_log(LOG_WARNING, "table and key too large lengh\n");
		return -1;
	}

	fullkey_len = snprintf(fullkey, sizeof(fullkey), "/%s/%s", table, key);

	spd_mutex_lock(&dblock);

	if(sqlite3_bind_text(get_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		spd_log(LOG_WARNING, "Couldnot bind key to stmt:%s\n", sqlite3_errmsg(spd_db));
		res = -1;
	} else if(sqlite3_step(get_stmt) != SQLITE_ROW) {
		spd_log(LOG_WARNING, "Unable to find key '%s' in table '%s' \n", key, table);
		res = -1;
	} else if (!(result = sqlite3_column_text(get_stmt, 0))) {
		spd_log(LOG_WARNING, "Couldn't get value\n");
		res = -1;
	} else {
		strncpy(buf, (const char *) result, len);
	}
	sqlite3_reset(get_stmt);
	spd_mutex_unlock(&dblock);

	return res;
}

int spd_db_put(const char * table, const char * key, const char * buf)
{
	char fullkey[256];
	size_t fullkey_len;
	int res = 0;

	if(strlen(table) + strlen(key) + 2 > sizeof(fullkey)) {
		spd_log(LOG_WARNING, "table and key too large lengh\n");
		return -1;
	}

	fullkey_len = snprintf(fullkey, sizeof(fullkey), "/%s/%s", table, key);

	spd_mutex_lock(&dblock);
	
	if(sqlite3_bind_text(put_stmt, 1, fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		spd_log(LOG_WARNING, "Could not bind key to stmt : %s\n", sqlite3_errmsg(spd_db));
		res = -1;
	} else if(sqlite3_bind_text(put_stmt, 2, buf, -1, SQLITE_STATIC) != SQLITE_OK) {
		spd_log(LOG_WARNING, "Could not bind value to stmt: %s\n", sqlite3_errmsg(spd_db));
		res = -1;
	} else if(sqlite3_step(put_stmt) != SQLITE_DONE) {
		spd_log(LOG_WARNING, "Could not execute statement: %s\n", sqlite3_errmsg(spd_db));
		res = -1;
	}
	
	/* make us reused */
	sqlite3_reset(put_stmt);
	do_dbsync();
	spd_mutex_unlock(&dblock);

	return res;
}

int spd_db_del(const char * table, const char * key)
{
	char fullkey[256];
	size_t fullkey_len;
	int res = 0;

	if(strlen(table) + strlen(key) + 2 > sizeof(fullkey)) {
		spd_log(LOG_WARNING, "table and key too large lengh\n");
		return -1;
	}

	fullkey_len = snprintf(fullkey, sizeof(fullkey), "/%s/%s", table, key);

	spd_mutex_lock(&dblock);
	if(sqlite3_bind_text(del_stmt, 1,fullkey, fullkey_len, SQLITE_STATIC) != SQLITE_OK) {
		spd_log(LOG_WARNING, "Could not bind key to stmt : %s\n", sqlite3_errmsg(spd_db));
		res = -1;
	} else if(sqlite3_step(del_stmt) != SQLITE_DONE) {
		spd_log(LOG_WARNING, "Unable to find key '%s' in table '%s'\n", key, table);
		res = -1;
	}

	sqlite3_reset(del_stmt);

	do_dbsync();
	spd_mutex_unlock(&dblock);

	return res;
}

struct spd_db_entry * spd_db_gettree(const char * table, const char * key)
{
	char prefix[256];
	sqlite3_stmt *stmt = gettree_stmt;
	struct spd_db_entry *cur, *last = NULL, *ret = NULL;

	if(!spd_strlen_zero(table)) {
		if(!spd_strlen_zero(table)) {
			snprintf(prefix, sizeof(prefix), "/%s/%s", table, key);
		} else {
			snprintf(prefix, sizeof(prefix), "/%s", table);
		}
	} else {
		prefix[0] = '\0';
		stmt = gettree_stmt;
	}

	spd_mutex_lock(&dblock);
	if(!spd_strlen_zero(prefix) && (sqlite3_bind_text(stmt, 1, prefix, -1, SQLITE_STATIC) != SQLITE_OK)) {
		spd_log(LOG_WARNING, "Could bind %s to stmt: %s\n", prefix, sqlite3_errmsg(spd_db));
		sqlite3_reset(stmt);
		spd_mutex_unlock(&dblock);
		return NULL;
	}

	while(sqlite3_step(stmt) == SQLITE_ROW) {
		const char *key_s, *value_s;
		if (!(key_s = (const char *) sqlite3_column_text(stmt, 0))) {
			break;
		}
		if (!(value_s = (const char *) sqlite3_column_text(stmt, 1))) {
			break;
		}

		if(!(cur = spd_malloc(sizeof(*cur) + strlen(key_s) + strlen(value_s) + 2))) {
			break;
		}

		cur->next = NULL;
		cur->key = cur->data + strlen(value_s) + 1;
		strcpy(cur->data, value_s);
		strcpy(cur->key, key_s);
		if(last) {
			last->next = cur;
		} else {
			ret = cur;
		}
		last = cur;
	}

	sqlite3_reset(stmt);
	spd_mutex_unlock(&dblock);

	return ret;
}

int spd_db_deltree(const char * table, const char * key)
{
	sqlite3_stmt *stmt = deltree_stmt;
	char prefix[256];
	int res = 0;

	if (!spd_strlen_zero(table)) {
		if (!spd_strlen_zero(key)) {
			/* Family and key tree */
			snprintf(prefix, sizeof(prefix), "/%s/%s", table, key);
		} else {
			/* Family only */
			snprintf(prefix, sizeof(prefix), "/%s", table);
		}
	} else {
		prefix[0] = '\0';
		stmt = deltree_all_stmt;
	}

	spd_mutex_lock(&dblock);
	if (!spd_strlen_zero(prefix) && (sqlite3_bind_text(stmt, 1, prefix, -1, SQLITE_STATIC) != SQLITE_OK)) {
		spd_log(LOG_WARNING, "Could bind %s to stmt: %s\n", prefix, sqlite3_errmsg(spd_db));
		res = -1;
	} else if (sqlite3_step(stmt) != SQLITE_DONE) {
		spd_log(LOG_WARNING, "Couldn't execute stmt: %s\n", sqlite3_errmsg(spd_db));
		res = -1;
	}
	res = sqlite3_changes(spd_db);
	sqlite3_reset(stmt);
	do_dbsync();
	spd_mutex_unlock(&dblock);

	return res;
}

void spd_db_freetree(struct spd_db_entry * entry)
{
	struct  spd_db_entry *last;

	while(entry) {
		last = entry;
		entry = entry->next;
		spd_safe_free(last);
	}
}

static int db_do_transaction(const char *sql, int(*callback)(void *, int, char **, char **), void *arg)
{
	char *errmsg = NULL;
	int ret = 0;
    
	sqlite3_exec(spd_db, sql, callback, arg, &errmsg);
	if(errmsg) {
		spd_log(LOG_WARNING, "Error excuting SQL :%s\n", errmsg);
		sqlite3_free(errmsg);
		ret = -1;
	}

	return ret;
}

static int spd_db_begin_transaction()
{
	return db_do_transaction("BEGIN TRANSACTION", NULL, NULL);
}
	 
static int spd_db_commite_trancaction()
{
	return db_do_transaction("COMMIT", NULL, NULL);
}

static int spd_db_rollback_transaction()
{
	db_do_transaction("ROLLBACK", NULL, NULL);
}

static void *dbsync_thread_loop(void *data)
{
	spd_mutex_lock(&dblock);
	spd_db_begin_transaction();
	spd_log(LOG_NOTICE, "db sync thread loop\n");

	for(;;) {
		spd_log(LOG_NOTICE, "wait sync cond \n");
		spd_cond_wait(&dbcond, &dblock);
		spd_log(LOG_NOTICE, "get sync cond \n");
		if(spd_db_commite_trancaction()) {
			spd_db_rollback_transaction();
		}
		if(doexit) {
			spd_mutex_unlock(&dblock);
			break;
		}

		spd_db_begin_transaction();
		spd_mutex_unlock(&dblock);
		sleep(1);
		spd_mutex_lock(&dblock);
		if(doexit) {
			spd_mutex_unlock(&dblock);
			break;
		}
	}

	return NULL;
}

int spddb_init(void)
{
	spd_log(LOG_NOTICE, "start init db engine...\n");
	if(db_init()) {
		spd_log(LOG_ERROR, "spd db init failed \n");
		return -1;
	}

	spd_cond_init(&dbcond, NULL);
	if(spd_pthread_create_background(&dbsync_thread, NULL, dbsync_thread_loop, NULL)) {
		spd_log(LOG_ERROR, "failed to start db thread. \n");
		return -1;
	}

	spd_log(LOG_NOTICE, "end init db engine...\n");
	return 0;
 }
 