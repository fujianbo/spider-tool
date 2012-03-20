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
  * sqlite3 c interface api
  * http://www.sqlite.org/c3ref/intro.html
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
SPD_DB_STATEMENT_DEFINE(gettree_stmt,"SELECT key, value FROM astdb WHERE key || '/' LIKE ? || '/' || '%' ORDER BY key")
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




/*
  * sync db signal , must lock db first.
  */
static void do_dbsync()
{
	spd_cond_signal(&dbcond);
}


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


void spddb_uninit(void)
{
	spd_mutex_lock(&dblock);
	do_dbsync();
	spd_mutex_unlock(&dblock);
	
	pthread_join(dbsync_thread, NULL);
	spd_mutex_lock(&dblock);
	spd_db_close(spd_db);
	spd_mutex_unlock(&dblock);
}

static int db_do_transaction(const char *sql, int(*callback)(void *, int, char **, char **), void *arg)
{
	char *errmsg = NULL;
	int ret = 0;


	sqlit3_exec(spd_db, sql, callback, arg, &errmsg);
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

int spd_db_get(const char * table, const char * key, char * buf, int len)
{
	char fullkey[256];
	size_t fullkey_len;
	int res = 0;

	if(strlen(table) + strlen(key) + 2 > sizeof(fullkey) -1) {
		spd_log(LOG_WARNING, "table and key too large \n");
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
		strncpy(value, (const char *) result, valuelen);
	}
	sqlite3_reset(get_stmt);
	spd_mutex_unlock(&dblock);

	return res;
}
static int spd_db_rollback_transaction()
{
	db_do_transaction("ROLLBACK", NULL, NULL);
}

static void dbsync_thread_loop(void *data)
{
	spd_mutex_lock(&dblock);
	spd_db_begin_transaction();


	for(;;) {
		spd_cond_wait(&dbcond, &dblock);
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
	if(db_init()) {
		spd_log(LOG_ERROR, "spd db init failed \n");
		return -1;
	}


	spd_cond_init(&dbcond, NULL);
	if(spd_pthread_create_background(&dbsync_thread, NULL, dbsync_thread_loop, NULL)) {
		spd_log(LOG_ERROR, "failed to start db thread. \n");
		return -1;
	}


	return 0;
 }
 