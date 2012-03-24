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
#ifndef _SPIDER_INTERNAL_DB_H
#define _SPIDER_INTERNAL_DB_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

struct spd_db_entry {
	struct spd_db_entry *next;
	char *key;
	char data[0];
};

/*!\brief Get key value specified by family/key */
int spd_db_get(const char *table, const char *key, char *buf, int len);

/*!\brief Store value addressed by family/key */
int spd_db_put(const char * table, const char *key, const char *buf);

/*!\brief Delete entry in db */
int spd_db_del(const char *table, const char *key);

/*!\brief Delete one or more entries in spddb
 * If both parameters are NULL, the entire database will be purged.  If
 * only keytree is NULL, all entries within the family will be purged.
 * It is an error for keytree to have a value when family is NULL.
 *
 * \retval -1 An error occurred
 * \retval >= 0 Number of records deleted
 */
int spd_db_deltree(const char *table, const char *key);

/*!\brief Get a list of values within the spddb tree
 * If family is specified, only those keys will be returned.  If keytree
 * is specified, subkeys are expected to exist (separated from the key with
 * a slash).  If subkeys do not exist and keytree is specified, the tree will
 * consist of either a single entry or NULL will be returned.
 *
 * Resulting tree should be freed by passing the return value to spd_db_freetree()
 * when usage is concluded.
 */
struct spd_db_entry *spd_db_gettree(const char *table, const char *key);

/*!\brief Free structure created by spd_db_gettree() */
void spd_db_freetree(struct spd_db_entry *entry);

/* init db engine at start time */
int spddb_init(void);
void spddb_uninit(void);
	
#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif