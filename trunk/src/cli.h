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
 * \brief Standard Command Line Interface
 */
 
#ifndef _SPIDER_CLI_H
#define _SPIDER_CLI_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "linkedlist.h"


/* dont check permissions while passing this option as a 'uid'
 * to the cli_has_permissions() function. */
#define CLI_NO_PERMS		-1

#define RESULT_SUCCESS		0
#define RESULT_SHOWUSAGE	1
#define RESULT_FAILURE		2

#define CLI_SUCCESS	(char *)RESULT_SUCCESS
#define CLI_SHOWUSAGE	(char *)RESULT_SHOWUSAGE
#define CLI_FAILURE	(char *)RESULT_FAILURE

#define SPD_MAX_CMD_LEN 	16

#define SPD_MAX_ARGS 64

#define SPD_CLI_COMPLETE_EOF	"_EOF_"

/*!
 * In many cases we need to print singular or plural
 * words depending on a count. This macro helps us e.g.
 *     printf("we have %d object%s", n, ESS(n));
 */
#define ESS(x) ((x) == 1 ? "" : "s")

/*! \brief return Yes or No depending on the argument.
 * This is used in many places in CLI command, having a function to generate
 * this helps maintaining a consistent output (and possibly emitting the
 * output in other languages, at some point).
 */
#define SPD_CLI_YESNO(x) (x) ? "Yes" : "No"

/*! \brief return On or Off depending on the argument.
 * This is used in many places in CLI command, having a function to generate
 * this helps maintaining a consistent output (and possibly emitting the
 * output in other languages, at some point).
 */
#define SPD_CLI_INOFF(x) (x) ? "On" : "Off"

enum spd_cli_command {
	CLI_USAGE,   /* return usage string */
	CLI_COMPLETE,    /* complete help  */
	CLI_HANDLER,      /* run handler */
};

struct spd_cli_arg {
	int fd;            /* client connect fd */
	int argc;
	const char *const *argv;
	const char *line; 
	const char *word;  /* complete word */
	const int pos;     /* position of the word complete */
	const int n;       /* the iteration count */
};

struct spd_cli_entry {
	const char *const cmda[SPD_MAX_CMD_LEN]; /*!< words making up the command. */
	const char *summary;                    /*!< Summary of the command (< 60 characters) */
	const char * usage; 				/*!< Detailed usage information */
	int inuse; 				/*!< For keeping track of usage */
	char *_full_cmd;			/*!< built at load time from cmda[] */
	int cmdlen;
	int args;				/*!< number of non-null entries in cmda */
	char *command;				/*!< command, non-null for new-style entries */
	char *(*handler)(struct spd_cli_entry *e, int cmd, struct spd_cli_arg *arg);
	SPD_LIST_ENTRY(spd_cli_entry)list;
};

#define SPD_CLI_DEFINE(callback, summar, ...) {.handler = callback, .summary = summar, ## __VA_ARGS__ }

/*!
 * Helper function to generate cli entries from a NULL-terminated array.
 * Returns the n-th matching entry from the array, or NULL if not found.
 * Can be used to implement generate() for static entries as below
 * (in this example we complete the word in position 2):
  \code
    char *my_generate(const char *line, const char *word, int pos, int n)
    {
        static const char * const choices[] = { "one", "two", "three", NULL };
	if (pos == 2)
        	return spd_cli_complete(word, choices, n);
	else
		return NULL;
    }
  \endcode
 */
char *spd_cli_complete(const char *word, const char *const choices[], int state);

/*! 
 * \brief Interprets a command
 * Interpret a command s, sending output to fd if uid:gid has permissions
 * to run this command. uid = CLI_NO_PERMS to avoid checking user permissions
 * gid = CLI_NO_PERMS to avoid checking group permissions.
 * \param uid User ID that is trying to run the command.
 * \param gid Group ID that is trying to run the command.
 * \param fd pipe
 * \param s incoming string
 * \retval 0 on success
 * \retval -1 on failure
 */
int spd_command_full(int uid, int gid, int fd, const char *a);
#define spd_cli_command(fd, s) spd_cli_command_full(CLI_NO_PERMS, CLI_NO_PERMS, fd, (s))

/*! 
 * \brief Executes multiple CLI commands
 * Interpret strings separated by NULL and execute each one, sending output to fd
 * if uid has permissions, uid = CLI_NO_PERMS to avoid checking users permissions.
 * gid = CLI_NO_PERMS to avoid checking group permissions.
 * \param uid User ID that is trying to run the command.
 * \param gid Group ID that is trying to run the command.
 * \param fd pipe
 * \param size is the total size of the string
 * \param s incoming string
 * \retval number of commands executed
 */
int spd_cli_command_multiple_full(int uid, int gid, int fd, size_t size, const char *s);

#define spd_cli_command_multiple(fd,size,s) spd_cli_command_multiple_full(CLI_NO_PERMS, CLI_NO_PERMS, fd, size, s)

/*! \brief Registers a command or an array of commands
 * \param e which cli entry to register.
 * Register your own command
 * \retval 0 on success
 * \retval -1 on failure
 */
int spd_cli_register(struct spd_cli_entry *e);

/*!
 * \brief Register multiple commands
 * \param e pointer to first cli entry to register
 * \param len number of entries to register
 */
int spd_cli_register_multiple(struct spd_cli_entry *e, int len);

/*! 
 * \brief Unregisters a command or an array of commands
 * \param e which cli entry to unregister
 * Unregister your own command.  You must pass a completed ast_cli_entry structure
 * \return 0
 */
int spd_cli_unregister(struct spd_cli_entry *e);

/*!
 * \brief Unregister multiple commands
 * \param e pointer to first cli entry to unregister
 * \param len number of entries to unregister
 */
int spd_cli_unregister_multiple(struct spd_cli_entry *e, int len);

/*! 
 * \brief Readline madness
 * Useful for readline, that's about it
 * \retval 0 on success
 * \retval -1 on failure
 */
char *spd_cli_generator(const char *, const char *, int);

int spd_cli_generatornummatches(const char *, const char *);

/*!
 * \brief Generates a NULL-terminated array of strings that
 * 1) begin with the string in the second parameter, and
 * 2) are valid in a command after the string in the first parameter.
 *
 * The first entry (offset 0) of the result is the longest common substring
 * in the results, useful to extend the string that has been completed.
 * Subsequent entries are all possible values, followed by a NULL.
 * All strings and the array itself are malloc'ed and must be freed
 * by the caller.
 */
char **spd_cli_completion_matches(const char *, const char *);

void spd_cli(int fd, const char *fmt, ...) __attribute__((format(printf, 2,3)));

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
