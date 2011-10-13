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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

#include "config.h"
#include "lock.h"
#include "logger.h"
#include "utils.h"
#include "strings.h"
#include "linkedlist.h"
#include "threadprivdata.h"
#include "const.h"


#define MAX_NEXT_COMMENT 128
#define COBUF_LEN  250
#define COMMENT_START  ";--"
#define COMMENT_END  "--"
#define COMMENT_META  ';'
#define COMMENT_TAG  '-'

static struct spd_config_map {
    struct spd_config_map *next;
    char *name;
    char *driver;
    char *database;
    char *table;
    char stuff[0];
 } *config_maps = NULL;


/* used by config_maps list */
SPD_MUTEX_DEFINE_STATIC(config_lock);

static struct spd_config_engine *config_engine_list;


static void comment_init(char **comment_buf, int *buf_size, char **line_buf, int 
    *line_bufer_size)
{
    if(!(*comment_buf)) {
        *comment_buf = spd_malloc(COBUF_LEN);
        if(!(*comment_buf))
            return;
        (*comment_buf)[0] = 0;
         *buf_size = COBUF_LEN;
         line_buf = spd_malloc(COBUF_LEN);
         if(!(*line_buf))
            return;
         (*line_buf)[0] = 0;
         *line_bufer_size = COBUF_LEN;
    } else {
        (*comment_buf)[0] = 0;
        (*line_buf)[0] = 0;
    }
}

static void comment_add(char **comment_buffer, int *comment_buffer_size, char *str)
{
    int rem = *comment_buffer_size - strlen(*comment_buffer) - 1;
	int siz = strlen(str);
	if (rem < siz+1) {
		*comment_buffer = spd_realloc(*comment_buffer, *comment_buffer_size + COBUF_LEN + siz + 1);
		if (!(*comment_buffer))
			return;
		*comment_buffer_size += COBUF_LEN+siz+1;
	}
	strcat(*comment_buffer,str);
}

static void  comment_add_len(char **comment_buffer, int *comment_buffer_size, char *str, int 
len)
{
	int cbl = strlen(*comment_buffer) + 1;
	int rem = *comment_buffer_size - cbl;
	if (rem < len+1) {
     *comment_buffer = spd_realloc(*comment_buffer, *comment_buffer_size + COBUF_LEN+ len + 1);
		if (!(*comment_buffer))
			return;
		*comment_buffer_size += COBUF_LEN+len+1;
	}
	strncat(*comment_buffer,str,len);
	(*comment_buffer)[cbl+len-1] = 0;
}


static void  llb_add(char **lline_buffer, int *lline_buffer_size, char *str)
{
	int rem = *lline_buffer_size - strlen(*lline_buffer) - 1;
	int siz = strlen(str);
	if (rem < siz+1) {
		*lline_buffer = spd_realloc(*lline_buffer, *lline_buffer_size + COBUF_LEN+ siz + 1);
		if (!(*lline_buffer)) 
			return;
		*lline_buffer_size += COBUF_LEN+ siz + 1;
	}
	strcat(*lline_buffer,str);
}

static void comment_reset(char **comment_buffer, char **lline_buffer)  
{ 
	(*comment_buffer)[0] = 0; 
	(*lline_buffer)[0] = 0;
}

static struct spd_comment *comment_alloc(const char *buf)
{
    struct spd_comment *cmt = spd_calloc(1,sizeof(struct spd_comment) + strlen(buf) + 1);
    strcpy(cmt->buf, buf);
    return cmt;
}

struct spd_variable *spd_variable_new(const char *name, const char *value)
{
    struct spd_variable *var;
    int name_len = strlen(name) + 1;

    if((var = spd_calloc(1, name_len + strlen(value) + 1 + sizeof(struct spd_variable)))) {
        var->name = var->buf;
        var->value = var->buf + name_len;
        strcpy(var->name, name);
        strcpy(var->value, value);
    }

    return var;
}

void spd_variable_append(struct spd_section * category, struct spd_variable * variable)
{
    if(!variable)
        return;
    if(category->last)
        category->last->next = variable;
    else
        category->root = variable;
    category->last = variable;
    while(category->last->next)
        category->last = category->last->next;
}

void spd_variables_destroy(struct spd_variable * var)
{
    struct spd_variable *v;

    while(var) {
        v = var;
        var = var->next;
        spd_safe_free(v);
    }
}

struct spd_variable *spd_variable_browse(const struct spd_config * config, const char * section)
{
    struct spd_section *cat = NULL;

    if(section && config->last_browse && (config->last_browse->name == section))
        cat = config->last_browse;
    else
        cat = spd_section_get(config, section);

    return (cat) ? cat->root : NULL;
}

const char *spd_config_option(struct spd_config *cfg, const char *cat, const char *var)
{
    const char *tmp;
    tmp = spd_variable_retrive(cfg, cat, var);
    if(!tmp)
        tmp = spd_variable_retrive(cfg, "general", var);

    return tmp;
}

const char *spd_variable_retrive(const struct spd_config * config, const char * section, const char * variable)
{
    struct spd_variable *v;

    if(section) {
        for(v = spd_variable_browse(config, section); v; v = v->next) {
            if(!strcasecmp(variable, v->name))
                return v->name;
        }
    } else {
        struct spd_section *cat;

        for(cat = config->root; cat; cat = cat->next)
            for(v = cat->root; v; v = v->next)
                if(!strcasecmp(variable, v->name))
                    return v->value;
    }

    return NULL;
}

static struct spd_variable *variable_clone(const struct spd_variable *old)
{
    struct spd_variable * newvar = spd_variable_new(old->name, old->value);

    if(newvar) {
        newvar->lineno = old->lineno;
        newvar->blanklines = old->blanklines;
    }

    return newvar;
}

static void move_variables(struct spd_section *old, struct spd_section *newsec)
{
    struct spd_variable *var = old->root;
    old->root = NULL;

    spd_variable_append(newsec, var);
}

struct spd_section *spd_section_new(const char *name)
{
    struct spd_section *sec;
    if((sec = spd_calloc(1, sizeof(*sec))))
        spd_copy_string(sec->name, name, sizeof(sec->name));

    return sec;
}

static struct spd_section *section_get(const struct spd_config *cfg, const char *name, 
    int ignored)
{
    struct spd_section *cat;
    for(cat = cfg->root; cat; cat = cat->next) {
        if(cat->name == name && (ignored || !cat->ignored))
            return cat;
    }

    for(cat = cfg->root; cat; cat = cat->next) {
        if(!strcasecmp(cat->name, name) && (ignored || !cat->ignored))
            return cat;
    }

    return NULL;
}

struct spd_section *spd_section_get(const struct spd_config *cfg, const char *name)
{
    return section_get(cfg, name, 0);
}

int spd_section_exist(const struct spd_config * config, const char * section_name)
{
    return !!spd_section_get(config, section_name);
}

void spd_section_append(struct spd_config * cfg, struct spd_section * cat)
{
    if(cfg->last)
        cfg->last->next = cat;
    else
        cfg->root = cat;
    cat->include_level = cfg->include_level;
    cfg->last = cat;
    cfg->current = cat;
}

static void spd_destroy_comment(struct spd_comment **cmt)
{
    struct spd_comment *n, *p;

    for(p = *cmt; p; p = n) {
        n = p->next;
        free(p);
    }

    *cmt = NULL;
}

static void spd_destroy_comments(struct spd_section *cat)
{
	spd_destroy_comment(&cat->prev);
	spd_destroy_comment(&cat->sameline);
}

static void spd_destroy_template_list(struct spd_section *cat)
{
	struct spd_section_template_instance *x;
	SPD_LIST_TRAVERSE_SAFE_BEGIN(&cat->template_instances, x, next) {
		SPD_LIST_REMOVE_CURRENT(&cat->template_instances, next);
		free(x);
	}
	SPD_LIST_TRAVERSE_SAFE_END;
}


void spd_section_destroy(struct spd_section *cat)
{
    spd_variables_destroy(cat->root);
    spd_destroy_comments(cat);
    spd_destroy_template_list(cat);
    spd_safe_free(cat);
}

static struct spd_section *next_available_section(struct spd_section *cat)
{
   for(; cat && cat->ignored; cat = cat->next);

   return cat;
}

struct spd_variable * spd_section_root(struct spd_config *cfg, char *cat)
{
    struct spd_section *sec = spd_section_get(cfg, cat);
    if(sec)
        return sec->root;
    return NULL;
}

char *spd_section_browse(struct spd_config *config, const char *prev)
{
    struct spd_section *sec = NULL;

    if(prev && config->last_browse && (config->last_browse->name == prev))
        sec = config->last_browse->next;
    else if(!prev && config->root)
        sec = config->root;
    else if(prev) {
        for(sec = config->root; sec; sec = sec->next) {
            if(sec->name == prev) {
                sec = sec->next;
                break;
            }
        }
        if(!sec) {
            for(sec = config->root; sec; sec = sec->next) {
                if(!strcasecmp(sec->name, prev)) {
                    sec = sec->next;
                    break;
                }
            }
        }
    }

    if(sec)
        sec = next_available_section(sec);

    config->last_browse = sec;
    return (sec) ? sec->name : NULL;
}

struct spd_variable *spd_section_detach_variables(struct spd_section *cat)
{
    struct spd_variable *v;

    v = cat->root;
    cat->root = NULL;
    cat->last = NULL;
}

void spd_section_rename(struct spd_section *cat, const char *name)
{
    spd_copy_string(cat->name, name, sizeof(cat->name));
}

static void inherit_section(struct spd_section *new, const struct spd_section *base)
{
	struct spd_variable *var;
	struct spd_section_template_instance *x = spd_calloc(1,sizeof(struct spd_section_template_instance));
	strcpy(x->name, base->name);
	x->inst = base;
	//SPD_LIST_INSERT_TAIL(&new->template_instances, x, next);
	for (var = base->root; var; var = var->next)
		spd_variable_append(new, variable_clone(var));
}


struct spd_config *spd_config_new(void)
{
    struct spd_config *config;

    if((config = spd_calloc(1, sizeof(*config))))
        config->max_include_level = 10;
    return config;
}

int spd_variable_delete(struct spd_section *category,  char *variable, char *match)
{
	struct spd_variable *cur, *prev=NULL, *curn;
	int res = -1;
	cur = category->root;
	while (cur) {
		if (cur->name == variable) {
			if (prev) {
				prev->next = cur->next;
				if (cur == category->last)
					category->last = prev;
			} else {
				category->root = cur->next;
				if (cur == category->last)
					category->last = NULL;
			}
			cur->next = NULL;
			spd_variables_destroy(cur);
			return 0;
		}
		prev = cur;
		cur = cur->next;
	}

	prev = NULL;
	cur = category->root;
	while (cur) {
		curn = cur->next;
		if (!strcasecmp(cur->name, variable) && (spd_strlen_zero(match) || 
          !strcasecmp(cur->value, match))) {
			if (prev) {
				prev->next = cur->next;
				if (cur == category->last)
					category->last = prev;
			} else {
				category->root = cur->next;
				if (cur == category->last)
					category->last = NULL;
			}
			cur->next = NULL;
			spd_variables_destroy(cur);
			res = 0;
		} else
			prev = cur;

		cur = curn;
	}
	return res;
}


int spd_variable_update(struct spd_section *category, const char *variable, 
	const char *value, const char *match, unsigned int object)
{
	struct spd_variable *cur, *prev=NULL, *newer;

	if (!(newer = spd_variable_new(variable, value)))
		return -1;
	
	newer->object = object;

	for (cur = category->root; cur; prev = cur, cur = cur->next) {
		if (strcasecmp(cur->name, variable) ||
			(!spd_strlen_zero(match) && strcasecmp(cur->value, match)))
			continue;

		newer->next = cur->next;
		newer->object = cur->object || object;
		if (prev)
			prev->next = newer;
		else
			category->root = newer;
		if (category->last == cur)
			category->last = newer;

		cur->next = NULL;
		spd_variables_destroy(cur);

		return 0;
	}

	if (prev)
		prev->next = newer;
	else
		category->root = newer;

	return 0;
}

int spd_section_delete(struct spd_config *cfg, const char *category)
{
	struct spd_section *prev=NULL, *cat;
	cat = cfg->root;
	while(cat) {
		if (cat->name == category) {
			if (prev) {
				prev->next = cat->next;
				if (cat == cfg->last)
					cfg->last = prev;
			} else {
				cfg->root = cat->next;
				if (cat == cfg->last)
					cfg->last = NULL;
			}
			spd_section_destroy(cat);
			return 0;
		}
		prev = cat;
		cat = cat->next;
	}

	prev = NULL;
	cat = cfg->root;
	while(cat) {
		if (!strcasecmp(cat->name, category)) {
			if (prev) {
				prev->next = cat->next;
				if (cat == cfg->last)
					cfg->last = prev;
			} else {
				cfg->root = cat->next;
				if (cat == cfg->last)
					cfg->last = NULL;
			}
			spd_section_destroy(cat);
			return 0;
		}
		prev = cat;
		cat = cat->next;
	}
	return -1;
}

void spd_config_destroy(struct spd_config *cfg)
{
	struct spd_section *cat, *catn;

	if (!cfg)
		return;

	cat = cfg->root;
	while(cat) {
		catn = cat;
		cat = cat->next;
		spd_section_destroy(catn);
	}
	spd_safe_free(cfg);
}

struct spd_section *spd_config_get_current_section(const struct spd_config *cfg)
{
	return cfg->current;
}

void spd_config_set_current_section(struct spd_config *cfg,  struct spd_section *cat)
{
	/* cast below is just to silence compiler warning about dropping "const" */
	cfg->current = (struct spd_section *) cat;
}

static int process_text_line(struct spd_config *cfg, struct spd_section **cat, 
    char *buf, int lineno, const char *configfile, int withcomments, char **comment_buffer, int *comment_buffer_size, 
    char **lline_buffer, int *lline_buffer_size)
{
	char *c;
	char *cur = buf;
	struct spd_variable *v;
	char cmd[512], exec_file[512];
	int object, do_exec, do_include;

	/* Actually parse the entry */
	if (cur[0] == '[') {
		struct spd_section *newcat = NULL;
		char *catname;

		/* A category header */
		c = strchr(cur, ']');
		if (!c) {
			spd_log(LOG_WARNING, "parse error: no closing ']', line %d of %s\n", lineno, configfile);
			return -1;
		}
		*c++ = '\0';
		cur++;
 		if (*c++ != '(')
 			c = NULL;
		catname = cur;
		if (!(*cat = newcat = spd_section_new(catname))) {
			return -1;
		}
		/* add comments */
		if (withcomments && *comment_buffer && (*comment_buffer)[0] ) {
			newcat->prev = comment_alloc(*comment_buffer);
		}
		if (withcomments && *lline_buffer && (*lline_buffer)[0] ) {
			newcat->sameline = comment_alloc(*lline_buffer);
		}
		if( withcomments )
			comment_reset(comment_buffer, lline_buffer);
		
 		/* If there are options or categories to inherit from, process them now */
 		if (c) {
 			if (!(cur = strchr(c, ')'))) {
 				spd_log(LOG_WARNING, "parse error: no closing ')', line %d of %s\n", lineno, configfile);
 				return -1;
 			}
 			*cur = '\0';
 			while ((cur = strsep(&c, ","))) {
				if (!strcasecmp(cur, "!")) {
					(*cat)->ignored = 1;
				} else if (!strcasecmp(cur, "+")) {
					*cat = section_get(cfg, catname, 1);
					if (!(*cat)) {
						if (newcat)
							spd_section_destroy(newcat);
spd_log(LOG_WARNING, "Category addition requested, but category %s does not exist, line %d of %s\n", catname, lineno, configfile);
						return -1;
					}
					if (newcat) {
						move_variables(newcat, *cat);
						spd_section_destroy(newcat);
						newcat = NULL;
					}
				} else {
					struct spd_section *base;
 				
					base = section_get(cfg, cur, 1);
					if (!base) {
spd_log(LOG_WARNING, "Inheritance requested, but category '%s' does not exist, line %d of %s\n", cur, lineno, configfile);
						return -1;
					}
					inherit_section(*cat, base);
				}
 			}
 		}
		if (newcat)
			spd_section_append(cfg, *cat);
	} else if (cur[0] == '#') {
		/* A directive */
		cur++;
		c = cur;
		while(*c && (*c > 32)) c++;
		if (*c) {
			*c = '\0';
			/* Find real argument */
			c = spd_strip(c + 1);
			if (!(*c))
				c = NULL;
		} else 
			c = NULL;
		do_include = !strcasecmp(cur, "include");
		if(!do_include)
			do_exec = !strcasecmp(cur, "exec");
		else
			do_exec = 0;
		if (do_exec) {
spd_log(LOG_WARNING, "Cannot perform #exec unless execincludes option is enabled in asterisk.conf (options section)!\n");
			do_exec = 0;
		}
		if (do_include || do_exec) {
			if (c) {
				cur = c;
				/* Strip off leading and trailing "'s and <>'s */
				if ((*c == '"') || (*c == '<')) {
					char quote_char = *c;
					if (quote_char == '<') {
						quote_char = '>';
					}

					if (*(c + strlen(c) - 1) == quote_char) {
						cur++;
						*(c + strlen(c) - 1) = '\0';
					}
				}
				/* #exec </path/to/executable>
				   We create a tmp file, then we #include it, then we delete it. */
				if (do_exec) { 
					snprintf(exec_file, sizeof(exec_file), "/var/tmp/exec.%d.%ld", (int)time(NULL), (long)pthread_self());
					snprintf(cmd, sizeof(cmd), "%s > %s 2>&1", cur, exec_file);
					spd_safe_system(cmd);
					cur = exec_file;
				} else
					exec_file[0] = '\0';
				/* A #include */
				//do_include = spd_config_internal_load(cur, cfg, withcomments) ? 1 : 0;
				if(!spd_strlen_zero(exec_file))
					unlink(exec_file);
				if (!do_include) {
					spd_log(LOG_ERROR, "*********************************************************\n");
					spd_log(LOG_ERROR, "*********** YOU SHOULD REALLY READ THIS ERROR ***********\n");
					spd_log(LOG_ERROR, "Future versions of Asterisk will treat a #include of a "
					                   "file that does not exist as an error, and will fail to "
					                   "load that configuration file.  Please ensure that the "
					                   "file '%s' exists, even if it is empty.\n", cur);
					spd_log(LOG_ERROR, "*********** YOU SHOULD REALLY READ THIS ERROR ***********\n");
					spd_log(LOG_ERROR, "*********************************************************\n");
					return 0;
				}

			} else {
				spd_log(LOG_WARNING, "Directive '#%s' needs an argument (%s) at line %d of %s\n", 
						do_exec ? "exec" : "include",
						do_exec ? "/path/to/executable" : "filename",
						lineno,
						configfile);
			}
		}
		else 
			spd_log(LOG_WARNING, "Unknown directive '#%s' at line %d of %s\n", cur, lineno, 
configfile);
	} else {
		/* Just a line (variable = value) */
		if (!(*cat)) {
			spd_log(LOG_WARNING,
				"parse error: No category context for line %d of %s\n", lineno, configfile);
			return -1;
		}
		c = strchr(cur, '=');
		if (c) {
			*c = 0;
			c++;
			/* Ignore > in => */
			if (*c== '>') {
				object = 1;
				c++;
			} else
				object = 0;
			if ((v = spd_variable_new(spd_strip(cur), spd_strip(c)))) {
				v->lineno = lineno;
				v->object = object;
				/* Put and reset comments */
				v->blanklines = 0;
				spd_variable_append(*cat, v);
				/* add comments */
				if (withcomments && *comment_buffer && (*comment_buffer)[0] ) {
					v->prev = comment_alloc(*comment_buffer);
				}
				if (withcomments && *lline_buffer && (*lline_buffer)[0] ) {
					v->sameline = comment_alloc(*lline_buffer);
				}
				if( withcomments )
					comment_reset(comment_buffer, lline_buffer);
				
			} else {
				return -1;
			}
		} else {
			spd_log(LOG_WARNING, "No '=' (equal sign) in line %d of %s\n", lineno, configfile);
		}
	}
	return 0;
}

static struct spd_config *config_text_file_load(const char *database, const char *table,
        const char *filename, struct spd_config *cfg, int withcomments)
{
    char fn[256];
    char buf[8192];

    char *new_buf, *comment_p, *process_buf;
	FILE *f;
	int lineno=0;
	int comment = 0, nest[MAX_NEXT_COMMENT];
	struct spd_section *cat = NULL;
	int count = 0;
	struct stat statbuf;
	/*! Growable string buffer */
	char *comment_buffer=0;   /*!< this will be a comment collector.*/
	int   comment_buffer_size=0;  /*!< the amount of storage so far alloc'd for the 
                                    					comment_buffer */

	char *lline_buffer=0;    /*!< A buffer for stuff behind the ; */
	int  lline_buffer_size=0;

        cat = spd_config_get_current_section(cfg);
    
        if (filename[0] == '/') {
            spd_copy_string(fn, filename, sizeof(fn));
        } else {
            snprintf(fn, sizeof(fn), "%s/%s", spd_config_SPD_CONFIG_DIR, filename);
        }
        if (withcomments) {
            comment_init(&comment_buffer, &comment_buffer_size, &lline_buffer, &lline_buffer_size);
            if (!lline_buffer || !comment_buffer) {
                spd_log(LOG_ERROR, "Failed to initialize the comment buffer!\n");
                return NULL;
            }
        }

        do {
            if (stat(fn, &statbuf))
                continue;
    
            if (!S_ISREG(statbuf.st_mode)) {
                spd_log(LOG_WARNING, "'%s' is not a regular file, ignoring\n", fn);
                continue;
            }
            if (option_verbose > 1) {
                spd_verbose(VERBOSE_PREFIX_2 "Parsing == %s ", fn);
                fflush(stdout);
            }
            if (!(f = fopen(fn, "r"))) {
                if (option_debug)
                    spd_log(LOG_DEBUG, "No file to parse: %s\n", fn);
                if (option_verbose > 1)
                    spd_verbose( "Not found (%s)\n", strerror(errno));
                continue;
            }
            count++;
            if (option_debug)
                spd_log(LOG_DEBUG, "Parsing %s\n", fn);
            if (option_verbose > 1)
                spd_verbose("Found\n");
            while(!feof(f)) {
                lineno++;
                if (fgets(buf, sizeof(buf), f)) {
                    if ( withcomments ) {
                        comment_add(&comment_buffer, &comment_buffer_size, lline_buffer);       
                        /* add the cuent lline buffer to the comment buffer */
                        lline_buffer[0] = 0;        /* erase the lline buffer */
                    }
                    
                    new_buf = buf;
                    if (comment) 
                        process_buf = NULL;
                    else
                        process_buf = buf;
                    
                    while ((comment_p = strchr(new_buf, COMMENT_META))) {
                        if ((comment_p > new_buf) && (*(comment_p-1) == '\\')) {
                            /* Escaped semicolons aren't comments. */
                            new_buf = comment_p + 1;
                        } else if(comment_p[1] == COMMENT_TAG && comment_p[2] == COMMENT_TAG && (comment_p[3] != '-')) {
                            /* Meta-Comment start detected ";--" */
                            if (comment < MAX_NEXT_COMMENT) {
                                *comment_p = '\0';
                                new_buf = comment_p + 3;
                                comment++;
                                nest[comment-1] = lineno;
                            } else {
                                spd_log(LOG_ERROR, "Maximum nest limit of %d reached.\n", MAX_NEXT_COMMENT);
                            }
                        } else if ((comment_p >= new_buf + 2) &&
                               (*(comment_p - 1) == COMMENT_TAG) &&
                               (*(comment_p - 2) == COMMENT_TAG)) {
                            /* Meta-Comment end detected */
                            comment--;
                            new_buf = comment_p + 1;
                            if (!comment) {
                                /* Back to non-comment now */
                                if (process_buf) {
                                    /* Actually have to move what's left over the top, then continue */
                                    char *oldptr;
                                    oldptr = process_buf + strlen(process_buf);
                                    if ( withcomments ) {
                                        comment_add(&comment_buffer, &comment_buffer_size, ";");
                                        comment_add_len(&comment_buffer, &comment_buffer_size, oldptr+1, new_buf-oldptr-1);
                                    }
                                    
                                    memmove(oldptr, new_buf, strlen(new_buf) + 1);
                                    new_buf = oldptr;
                                } else
                                    process_buf = new_buf;
                            }
                        } else {
                            if (!comment) {
                                /* If ; is found, and we are not nested in a comment, 
                                   we immediately stop all comment processing */
                                if ( withcomments ) {
                                    llb_add(&lline_buffer, &lline_buffer_size, comment_p);
                                }
                                *comment_p = '\0'; 
                                new_buf = comment_p;
                            } else
                                new_buf = comment_p + 1;
                        }
                    }
                    if( withcomments && comment && !process_buf )
                    {
                        comment_add(&comment_buffer, &comment_buffer_size, buf);  /* the whole line is a comment, store it */
                    }
                    
                    if (process_buf) {
                        char *buf = spd_strip(process_buf);
                        if (!spd_strlen_zero(buf)) {
                            if (process_text_line(cfg, &cat, buf, lineno, fn, withcomments, &comment_buffer, &
    comment_buffer_size, &lline_buffer, &lline_buffer_size)) {
                                cfg = NULL;
                                break;
                            }
                        }
                    }
                }
            }
            fclose(f);      
        } while(0);
        if (comment) {
            spd_log(LOG_WARNING,"Unterminated comment detected beginning on line %d\n", nest[comment - 1]);
        }

    
        if (cfg && cfg->include_level == 1 && withcomments && comment_buffer) {
            free(comment_buffer);
            free(lline_buffer);
            comment_buffer = NULL;
            lline_buffer = NULL;
            comment_buffer_size = 0;
            lline_buffer_size = 0;
        }
        
        if (count == 0)
            return NULL;
    
        return cfg;
    
}

int config_text_file_save(const char *configfile, const struct spd_config *cfg, const char *generator)
{
	FILE *f = NULL;
	int fd = -1;
	char fn[256], fntmp[256];
	char date[256]="";
	time_t t;
	struct spd_variable *var;
	struct spd_section *cat;
	struct spd_comment *cmt;
	struct stat s;
	int blanklines = 0;
	int stat_result = 0;

	if (configfile[0] == '/') {
		snprintf(fntmp, sizeof(fntmp), "%s.XXXXXX", configfile);
		spd_copy_string(fn, configfile, sizeof(fn));
	} else {
		snprintf(fntmp, sizeof(fntmp), "%s/%s.XXXXXX", spd_config_SPD_CONFIG_DIR, configfile);
		snprintf(fn, sizeof(fn), "%s/%s", spd_config_SPD_CONFIG_DIR, configfile);
	}
	time(&t);
	spd_copy_string(date, ctime(&t), sizeof(date));
	if ((fd = mkstemp(fntmp)) > 0 && (f = fdopen(fd, "w")) != NULL) {
		if (option_verbose > 1)
			spd_verbose(VERBOSE_PREFIX_2 "Saving '%s': ", fn);
		fprintf(f, ";!\n");
		fprintf(f, ";! Automatically generated configuration file\n");
		if (strcmp(configfile, fn))
			fprintf(f, ";! Filename: %s (%s)\n", configfile, fn);
		else
			fprintf(f, ";! Filename: %s\n", configfile);
		fprintf(f, ";! Generator: %s\n", generator);
		fprintf(f, ";! Creation Date: %s", date);
		fprintf(f, ";!\n");
		cat = cfg->root;
		while(cat) {
			/* Dump section with any appropriate comment */
			for (cmt = cat->prev; cmt; cmt=cmt->next)
			{
				if (cmt->buf[0] != ';' || cmt->buf[1] != '!')
					fprintf(f,"%s", cmt->buf);
			}
			if (!cat->prev)
				fprintf(f,"\n");
			fprintf(f, "[%s]", cat->name);
			if (cat->ignored || !SPD_LIST_EMPTY(&cat->template_instances)) {
				fprintf(f, "(");
				if (cat->ignored) {
					fprintf(f, "!");
				}
				if (cat->ignored && !SPD_LIST_EMPTY(&cat->template_instances)) {
					fprintf(f, ",");
				}
				if (!SPD_LIST_EMPTY(&cat->template_instances)) {
					struct spd_section_template_instance *x;
					SPD_LIST_TRAVERSE(&cat->template_instances, x, next) {
						fprintf(f,"%s",x->name);
						if (x != SPD_LIST_LAST(&cat->template_instances))
							fprintf(f,",");
					}
				}
				fprintf(f, ")");
			}

			for(cmt = cat->sameline; cmt; cmt=cmt->next)
			{
				fprintf(f,"%s", cmt->buf);
			}
			if (!cat->sameline)
				fprintf(f,"\n");
			var = cat->root;
			while(var) {
				struct spd_section_template_instance *x;
				struct spd_variable *v2;
				int found = 0;
				SPD_LIST_TRAVERSE(&cat->template_instances, x, next) {
					
					for (v2 = x->inst->root; v2; v2 = v2->next) {
						if (!strcasecmp(var->name, v2->name))
							break;
					}
					if (v2 && v2->value && !strcmp(v2->value, var->value)) {
						found = 1;
						break;
					}
				}
				if (found) {
					var = var->next;
					continue;
				}
				for (cmt = var->prev; cmt; cmt=cmt->next)
				{
					if (cmt->buf[0] != ';' || cmt->buf[1] != '!')
						fprintf(f,"%s", cmt->buf);
				}
				if (var->sameline) 
					fprintf(f, "%s %s %s  %s", var->name, (var->object ? "=>" : "="), var->value, 
					var->sameline->buf);
				else	
					fprintf(f, "%s %s %s\n", var->name, (var->object ? "=>" : "="), var->value);
				if (var->blanklines) {
					blanklines = var->blanklines;
					while (blanklines--)
						fprintf(f, "\n");
				}
					
				var = var->next;
			}
#if 0
			/* Put an empty line */
			fprintf(f, "\n");
#endif
			cat = cat->next;
		}
		if ((option_verbose > 1) && !option_debug)
			spd_verbose("Saved\n");
	} else {
		if (option_debug)
			spd_log(LOG_DEBUG, "Unable to open for writing: %s (%s)\n", fn, strerror(errno));
		if (option_verbose > 1)
			spd_verbose(VERBOSE_PREFIX_2 "Unable to write %s (%s)", fn, strerror(errno));
		if (fd > -1)
			close(fd);
		return -1;
	}
	if (!(stat_result = stat(fn, &s))) {
		fchmod(fd, s.st_mode);
	}
	fclose(f);
	if ((!stat_result && unlink(fn)) || link(fntmp, fn)) {
		if (option_debug)
			spd_log(LOG_DEBUG, "Unable to open for writing: %s (%s)\n", fn, strerror(errno));
		if (option_verbose > 1)
			spd_verbose(VERBOSE_PREFIX_2 "Unable to write %s (%s)", fn, strerror(errno));
		unlink(fntmp);
		return -1;
	}
	unlink(fntmp);
	return 0;
}


static void clear_config_maps(void)
{
    struct spd_config_map *map;

    spd_mutex_lock(&config_lock);

    while(config_maps) {
        map = config_maps;
        config_maps = config_maps->next;
        free(map);
    }
    spd_mutex_unlock(&config_lock);
}

static int append_mapping(char *name, char *driver, char *database, char *table)
{
    struct spd_config_map *map;
    int length;

    length = sizeof(*map) + strlen(name) +strlen(driver) + strlen(database) + 3;
    if(table)
        length += strlen(table) + 1;

    if(!(map = spd_calloc(1, length)))
        return -1;

    map->name = map->stuff;
    strcpy(map->name, name);
    map->driver = map->name + strlen(map->name) + 1;
	strcpy(map->driver, driver);
	map->database = map->driver + strlen(map->driver) + 1;
	strcpy(map->database, database);
	if (table) {
		map->table = map->database + strlen(map->database) + 1;
		strcpy(map->table, table);
	}
	map->next = config_maps;

	if (option_verbose > 1)
		spd_verbose(VERBOSE_PREFIX_2 "Binding %s to %s/%s/%s\n",
			    map->name, map->driver, map->database, map->table ? map->table : map->name);

	config_maps = map;
	return 0;
}


int spd_config_engine_register(struct spd_config_engine *engine)
{
    struct spd_config_engine *ptr;

    spd_mutex_lock(&config_lock);
    if(!config_engine_list) {
        config_engine_list =  engine;
    } else {
        for(ptr = config_engine_list; ptr->next; ptr = ptr->next);
        ptr->next = engine;
    }

    spd_mutex_unlock(&config_lock);
    spd_log(LOG_NOTICE, "register new engine %s\n", engine->name);

    return 1;
}

int spd_config_engine_deregister(struct spd_config_engine *del)
{
    struct spd_config_engine *ptr, *last = NULL;

    spd_mutex_lock(&config_lock);

    for(ptr = config_engine_list; ptr; ptr = ptr->next) {
        if(ptr == del) {
            if(last)
                last->next = ptr->next;
            else
                config_engine_list = ptr->next;
            break;
        }
        last = ptr;
    }
    spd_mutex_unlock(&config_lock);

    return 0;
}

static struct spd_config_engine *find_engine(const char *family,char *database, int dbsiz, char *table, int tabsiz)
{
    struct spd_config_engine *eng, *ret = NULL;
    struct spd_config_map *map;

    spd_mutex_lock(&config_lock);
    for(map = config_maps; map; map = map->next) {
        if(!strcasecmp(family, map->name)) {
            if(database)
                spd_copy_string(database, map->database, dbsiz);
            if(table)
                spd_copy_string(table, map->table ? map->table :family, tabsiz);
            break;
        }
    }
    if(map) {
        for(eng = config_engine_list; !ret && eng; eng = eng->next) {
            if(!strcasecmp(eng->name, map->driver))
                ret = eng;
        }
    }
    spd_mutex_unlock(&config_lock);
    if(map && !ret)
        spd_log(LOG_WARNING, "realtime mapping for  found to engine  , but engine not find \n");

    return ret;
}

static struct spd_config_engine text_file_engine = {
    .name = "text",
    .load_func = config_text_file_load,
};

struct spd_config *spd_config_internal_load(const char *filename, struct spd_config *cfg, int withcomments)
{   
    char db[256];
    char table[256];
    struct spd_config_engine *loader = &text_file_engine;
    struct spd_config *result = NULL;

    cfg->include_level++;

    if(config_engine_list) {
        struct spd_config_engine *eng;
        eng = find_engine(filename, db, sizeof(db), table, sizeof(table));

        if(eng && eng->load_func) {
			spd_log(LOG_NOTICE, "find engine name %s\n", eng->name);
            loader = eng;
        } else {
            eng = find_engine("global", db, sizeof(db), table, sizeof(table));
            if(eng && eng->load_func)
                loader = eng;
        }
    }
    result = loader->load_func(db, table, filename, cfg, withcomments);
    if(result)
        result->include_level--;
    else
        cfg->include_level--;

    return result;
    
}

struct spd_config *spd_config_load(const char *filename)
{
    struct spd_config *cfg;
    struct spd_config *result;

    cfg = spd_config_new();
    if(!cfg)
        return NULL;
    result = spd_config_internal_load(filename, cfg, 0);
    if(!result)
        spd_config_destroy(cfg);

    return result;
}

struct spd_config *spd_config_load_with_comments(const char *filename)
{
    struct spd_config *cfg;
    struct spd_config *result;

    cfg = spd_config_new();
    if(!cfg)
        return NULL;
    result = spd_config_internal_load(filename, cfg, 1);
    if(!result)
        spd_config_destroy(cfg);

    return result;
}
