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


#ifndef _SPIDER_CONFIG_H
#define _SPIDER_CONFIG_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "utils.h"
#include "strings.h"
#include "linkedlist.h"

/* define for comment */
struct spd_comment {
    struct spd_comment *next;
    char buf[0];
};

/* spider variable define , used for database colum and config file */
struct spd_variable {
    char *name;
    char *value;
    struct spd_variable *next;
    int lineno;
    int object;
    int blanklines;
    struct spd_comment *prev;
    struct spd_comment *sameline;
    struct spd_comment *tail;
    char buf[0];
};

struct spd_section_template_instance {
	char name[80]; /* redundant? */
	const struct spd_section *inst;
	SPD_LIST_ENTRY(spd_section_template_instance) next;
};

struct spd_section {
    char name[80];
    int ignored;
    int include_level;
    SPD_LIST_HEAD_NOLOCK(template_instance_list, spd_section_template_instance) template_instances;
    struct spd_comment  *prev;
    struct spd_comment  *sameline;
    struct spd_variable *root;
    struct spd_variable *last;
    struct spd_section  *next;
};

struct spd_config {
    struct spd_section *root;
    struct spd_section *last;
    struct spd_section *current;
    struct spd_section *last_browse;
    int include_level;
    int max_include_level;
};

typedef struct spd_config *load_config_func(const char *db, const char *table, const char *configfile, struct spd_config * config,int withcomments);

struct spd_config_engine {
    char *name;
    load_config_func *load_func;
    struct spd_config_engine *next;
};

/*! \brief Load a config file
 * \param filename path of file to open.  If no preceding '/' character, path is 
considered relative to SPD_CONFIG_DIR
 * Create a config structure from a given configuration file.
 * \param who_asked The module which is making this request.
 * \param flags Optional flags:
 * CONFIG_FLAG_WITHCOMMENTS - load the file with comments intact;
 * CONFIG_FLAG_FILEUNCHANGED - check the file mtime and return 
CONFIG_STATUS_FILEUNCHANGED if the mtime is the same; or
 * CONFIG_FLAG_NOCACHE - don't cache file mtime (main purpose of this option is to save 
memory on temporary files).
 *
 * \return an spd_config data structure on success
 * \retval NULL on error
 */
struct spd_config *spd_config_load(const char *filename);

//#define spd_load_config(filename)  spd_config_load(filename)

/*! \brief Destroys a config
 * \param config pointer to config data structure
 * Free memory associated with a given config
 *
 */
void  spd_config_destroy(struct spd_config *config);

/*! \brief returns the root ast_variable of a config
 * \param config pointer to an spd_config data structure
 * \param cat name of the category for which you want the root
 *
 * Returns the category specified
 */
struct spd_variable *spd_section_root(struct spd_config *config, char *section);

/*! \brief Goes through categories
 * \param config Which config structure you wish to "browse"
 * \param prev A pointer to a previous category.
 * This function is kind of non-intuitive in it's use.  To begin, one passes NULL as 
   the second argument.  It will return a pointer to the string of the first category in 
   the file.  From here on after, one must then pass the previous usage's return value as 
   the second pointer, and it will return a pointer to the category name afterwards.
 *
 * \retval a category on success
 * \retval NULL on failure/no-more-categories
 */
 char *spd_section_browse(struct spd_config *config, const char *prev);

/*!
 * \brief Goes through variables
 * Somewhat similar in intent as the spd_section_browse.
 * List variables of config file category
 *
 * \retval ast_variable list on success
 * \retval NULL on failure
 */
struct spd_variable *spd_variable_browse(const struct spd_config *config, const char 
    *section);

/*!
 * \brief given a pointer to a category, return the root variable.
 * This is equivalent to spd_variable_browse(), but more efficient if we
 * already have the struct spd_section * (e.g. from spd_section_get())
 */
struct spd_variable *spd_section_first(struct spd_section *cat);

/*!
 * \brief Gets a variable
 * \param config which (opened) config to use
 * \param category category under which the variable lies
 * \param variable which variable you wish to get the data for
 * Goes through a given config file in the given category and searches for the given 
    variable
 *
 * \retval The variable value on success
 * \retval NULL if unable to find it.
 */
const char *spd_variable_retrive(const struct spd_config *config, const char *section,
    const char *varialbe);

struct spd_section *spd_section_get(const struct spd_config *config, const char 
    *sectionname);

/*!
 * \brief Check for category duplicates
 * \param config which config to use
 * \param category_name name of the category you're looking for
 * This will search through the categories within a given config file for a match.
 *
 * \return non-zero if found
 */
int spd_section_exist(const struct spd_config *config, const char *section_name);



/*! \brief Free variable list
 * \param var the linked list of variables to free
 * This function frees a list of variables.
 */
void spd_variables_destroy(struct spd_variable *var);

/*! register config engine
 * retval 1 
 */
int spd_config_engine_register(struct spd_config_engine *cfg);

/*! unregister config engine
 * retval 1
 */
 int spd_config_engine_deregister(struct spd_config_engine *cfg);

/*!\brief Exposed initialization method for core process
 * This method is intended for use only with the core initialization and is
 * not designed to be called from any user applications.
 */
int register_config_cli(void);

/*!\brief Exposed re-initialization method for core process
 * This method is intended for use only with the core re-initialization and is
 * not designed to be called from any user applications.
 */
int read_config_maps(void);

/*!\brief Create a new base configuration structure */
struct spd_config *spd_config_new(void);

/*!\brief Retrieve the current category name being built.
 * API for backend configuration engines while building a configuration set.
 */
struct spd_section *spd_config_get_current_section(const struct spd_config *cfg);

/*!\brief Set the category within the configuration as being current.
 * API for backend configuration engines while building a configuration set.
 */
void spd_config_set_current_section(struct spd_config *cfg, struct spd_section *sec);

/*!\brief Retrieve a configuration variable within the configuration set.
 * Retrieves the named variable \p var within category \p cat of configuration
 * set \p cfg.  If not found, attempts to retrieve the named variable \p var
 * from within category \em general.
 * \return Value of \p var, or NULL if not found.
 */
const char *spd_config_option(struct spd_config *cfg, const char *cat, const char *var);

/*!\brief Create a category structure */
struct spd_section *spd_section_new(const char *name);

void spd_section_append(struct spd_config *cfg, struct spd_section *cat);

void spd_section_insert(struct spd_config *cfg, struct spd_section *cat, const char 
    *match);

int spd_section_delete(struct spd_config *cfg, const char *section);

/*!\brief Removes and destroys all variables within a category
 * \retval 0 if the category was found and emptied
 * \retval -1 if the category was not found
 */
int spd_section_empty(struct spd_config *cfg, const char *category);
void spd_section_destroy(struct spd_section *cat);
struct spd_variable *spd_section_detach_variables(struct spd_section *cat);
void spd_section_rename(struct spd_section *cat,  const char *name);


struct spd_variable *spd_variable_new(const char *name, const char *value);

void spd_variable_append(struct spd_section *category, struct spd_variable *variable);
void spd_variable_insert(struct spd_section *category, struct spd_variable *variable, 
    const char *line);
int spd_variable_delete(struct spd_section *category, char *variable,  char *match);

/*! \brief Update variable value within a config
 * \param category Category element within the config
 * \param variable Name of the variable to change
 * \param value New value of the variable
 * \param match If set, previous value of the variable (if NULL or zero-length, no 
    matching will be done)
 * \param object Boolean of whether to make the new variable an object
 * \return 0 on success or -1 on failure.
 */
int spd_variable_update(struct spd_section *category, const char *variable,
						const char *value, const char *match, unsigned int object);

int spd_config_text_file_save(const char *filename, const struct spd_config *cfg, const 
    char *generator);
int config_text_file_save(const char *filename, const struct spd_config *cfg, const 
    char *generator) __attribute__((deprecated));

struct spd_config *ast_config_internal_load(const char *configfile, struct spd_config *cfg, int wi);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif


#endif