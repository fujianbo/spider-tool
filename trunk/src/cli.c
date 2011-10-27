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

#include "linkedlist.h"
#include "cli.h"
#include "lock.h"
#include "threadprivdata.h"
#include "str.h"
 
SPD_THREADPRIVDATA(spd_cli_buf);

static SPD_RWLIST_HEAD_STATIC(spd_cli_entrys, spd_cli_entry);

#define SPD_CLI_BUF_LEN    256

/*!
 * Some regexp characters in cli arguments are reserved and used as separators.
 */
static const char cli_rsvd[] = "[]{}|*%";

void spd_cli(int fd, const char *fmt, ...)
{
 	int res;
	struct spd_str *buf;
	va_list ap;

	if(!(buf = spd_str_thread_get(&spd_cli_buf, SPD_CLI_BUF_LEN))){
		return;
	}

	va_start(ap, fmt);
	res = spd_str_set_va(&buf, 0, fmt, ap);
	va_end(ap);

	if(res != SPD_STR_BUILD_FAILED) {
		spd_timeout_write(fd, spd_str_buffer(buf), spd_str_len(buf), 100); /* 100 ms default */
	}
}

static struct spd_cli_entry *cli_next(struct spd_cli_entry *e)
{
	if(e) {
		SPD_LIST_NEXT(e,list);
	} else {
		SPD_LIST_FIRST(&spd_cli_entrys);
	}
}

static char *__spd_cli_generator(const char *text, const char *word, int state, int lock);

/*
 * helper function to generate CLI matches from a fixed set of values.
 * A NULL word is acceptable.
 */
char *spd_cli_complete(const char * word, const char * const choices[ ], int state)
{
	int i, which = 0, len;
	len = spd_strlen_zero(word) ? 0 : strlen(word);
	for(i = 0; choices[i]; i++) {
		if((!len || !strncasecmp(word, choices[i], len)) && ++ which > state)
			return spd_strdup(choices[i]);
	}

	return NULL;
}

static int spd_cli_set_full_cmd(struct spd_cli_entry *e)
{
	int i;
	char buf[80];
	spd_str_join(buf, sizeof(buf), e->cmda);

	e->_full_cmd = spd_strdup(buf);
	if(!e->_full_cmd) {
		return -1;
	}
	e->cmdlen = strcspn(e->_full_cmd, cli_rsvd);
	for(i = 0; e->cmda[i]; i++)
		;
	e->args = i;
	
	return 0;
}


/*!
 * match a word in the CLI entry.
 * returns -1 on mismatch, 0 on match of an optional word,
 * 1 on match of a full word.
 *
 * The pattern can be
 *   any_word           match for equal
 *   [foo|bar|baz]      optionally, one of these words
 *   {foo|bar|baz}      exactly, one of these words
 *   %                  any word
 */
static int word_match(const char *cmd, const char *cli_word)
{
	int l;
	char *pos;

	if (spd_strlen_zero(cmd) || spd_strlen_zero(cli_word))
		return -1;
	if (!strchr(cli_rsvd, cli_word[0])) /* normal match */
		return (strcasecmp(cmd, cli_word) == 0) ? 1 : -1;
	l = strlen(cmd);
	/* wildcard match - will extend in the future */
	if (l > 0 && cli_word[0] == '%') {
		return 1;	/* wildcard */
	}

	/* Start a search for the command entered against the cli word in question */
	pos = strcasestr(cli_word, cmd);
	while (pos) {

		/*
		 *Check if the word matched with is surrounded by reserved characters on both sides
		 * and isn't at the beginning of the cli_word since that would make it check in a location we shouldn't know about.
		 * If it is surrounded by reserved chars and isn't at the beginning, it's a match.
		 */
		if (pos != cli_word && strchr(cli_rsvd, pos[-1]) && strchr(cli_rsvd, pos[l])) {
			return 1;	/* valid match */
		}

		/* Ok, that one didn't match, strcasestr to the next appearance of the command and start over.*/
		pos = strcasestr(pos + 1, cmd);
	}
	/* If no matches were found over the course of the while loop, we hit the end of the string. It's a mismatch. */
	return -1;
}

/*! \brief if word is a valid prefix for token, returns the pos-th
 * match as a malloced string, or NULL otherwise.
 * Always tell in *actual how many matches we got.
 */
static char *is_prefix(const char *word, const char *token,
	int pos, int *actual)
{
	int lw;
	char *s, *t1;

	*actual = 0;
	if (spd_strlen_zero(token))
		return NULL;
	if (spd_strlen_zero(word))
		word = "";	/* dummy */
	lw = strlen(word);
	if (strcspn(word, cli_rsvd) != lw)
		return NULL;	/* no match if word has reserved chars */
	if (strchr(cli_rsvd, token[0]) == NULL) {	/* regular match */
		if (strncasecmp(token, word, lw))	/* no match */
			return NULL;
		*actual = 1;
		return (pos != 0) ? NULL : spd_strdup(token);
	}
	/* now handle regexp match */

	/* Wildcard always matches, so we never do is_prefix on them */

	t1 = spd_strdupa(token + 1);	/* copy, skipping first char */
	while (pos >= 0 && (s = strsep(&t1, cli_rsvd)) && *s) {
		if (*s == '%')	/* wildcard */
			continue;
		if (strncasecmp(s, word, lw))	/* no match */
			continue;
		(*actual)++;
		if (pos-- == 0)
			return spd_strdup(s);
	}
	return NULL;
}


/*!
 * \internal
 * \brief locate a cli command in the 'helpers' list (which must be locked).
 *     The search compares word by word taking care of regexps in e->cmda
 *     This function will return NULL when nothing is matched, or the spd_cli_entry that matched.
 * \param cmds
 * \param match_type has 3 possible values:
 *      0       returns if the search key is equal or longer than the entry.
 *		            note that trailing optional arguments are skipped.
 *      -1      true if the mismatch is on the last word XXX not true!
 *      1       true only on complete, exact match.
 *
 */
static struct spd_cli_entry *find_cli(const char * const cmds[], int match_type)
{
	int matchlen = -1;	/* length of longest match so far */
	struct spd_cli_entry *cand = NULL, *e=NULL;

	while ( (e = cli_next(e)) ) {
		/* word-by word regexp comparison */
		const char * const *src = cmds;
		const char * const *dst = e->cmda;
		int n = 0;
		for (;; dst++, src += n) {
			n = word_match(*src, *dst);
			if (n < 0)
				break;
		}
		if (spd_strlen_zero(*dst) || ((*dst)[0] == '[' && spd_strlen_zero(dst[1]))) {
			/* no more words in 'e' */
			if (spd_strlen_zero(*src))	/* exact match, cannot do better */
				break;
			/* Here, cmds has more words than the entry 'e' */
			if (match_type != 0)	/* but we look for almost exact match... */
				continue;	/* so we skip this one. */
			/* otherwise we like it (case 0) */
		} else {	/* still words in 'e' */
			if (spd_strlen_zero(*src))
				continue; /* cmds is shorter than 'e', not good */
			/* Here we have leftover words in cmds and 'e',
			 * but there is a mismatch. We only accept this one if match_type == -1
			 * and this is the last word for both.
			 */
			if (match_type != -1 || !spd_strlen_zero(src[1]) ||
			    !spd_strlen_zero(dst[1]))	/* not the one we look for */
				continue;
			/* good, we are in case match_type == -1 and mismatch on last word */
		}
		if (src - cmds > matchlen) {	/* remember the candidate */
			matchlen = src - cmds;
			cand = e;
		}
	}

	return e ? e : cand;
}

static char *find_best(const char *argv)
{
	static char cmdline[80];
	int x;
	const char *myargv[SPD_MAX_CMD_LEN] = {NULL,};

	SPD_RWLIST_RDLOCK(&spd_cli_entrys);
	for(x = 0; argv[x]; x++) {
		myargv[x] = argv[x];
		if(!find_cli(myargv, -1))
			break;
	}
	SPD_RWLIST_UNLOCK(&spd_cli_entrys);
	spd_str_join(cmdline, sizeof(cmdline), myargv);

	return cmdline;
}

static int __spd_cli_unregister(struct spd_cli_entry *e)
{
	if(e->inuse) {
		spd_log(LOG_WARNING, "Can not remove cli entry insue \n");
		return -1;
	}

	SPD_RWLIST_WRLOCK(&spd_cli_entrys);
	SPD_RWLIST_REMOVE(&spd_cli_entrys, e, list);
	SPD_RWLIST_UNLOCK(&spd_cli_entrys);

	spd_safe_free(e->_full_cmd);

	if(e->handler) {
		/* this is a new-style entry. Reset fields and free memory. */
		char *cmda = (char *) e->cmda;
		memset(cmda, '\0', sizeof(e->cmda));
		spd_safe_free(e->command);
		e->command = NULL;
		e->usage = NULL;
	}
}

static int __spd_cli_register(struct spd_cli_entry *e)
{
	struct spd_cli_entry *cur;
	int i, lf, ret = -1;

	struct spd_cli_arg a;

	char **dst = (char **)e->cmda;
	char *s;

	memset(&a, '\0', sizeof(a));

	e->handler(e, CLI_USAGE, &a);
	s = spd_skip_blanks(e->command);
	s = e->command = spd_strdup(s);

	for(i = 0; !spd_strlen_zero(s) && i < SPD_MAX_CMD_LEN -1; i++) {
		*dst++ = s;	/* store string */
		s = spd_skip_nonblanks(s);
		if (*s == '\0')	/* we are done */
			break;
		*s++ = '\0';
		s = spd_skip_blanks(s);
	}
	
	*dst++ = NULL;

	SPD_RWLIST_WRLOCK(&spd_cli_entrys);

	if(find_cli(e->cmda, 1)) {
		spd_log(LOG_WARNING, "Commad '%s' already registerd ", F_OR(e->_full_cmd, e->command));
		goto done;
	}

	if(spd_cli_set_full_cmd(e))
		goto done;

	lf = e->cmdlen;
	SPD_RWLIST_TRAVERSE_SAFE_BEGIN(&spd_cli_entrys, cur, list) {
		int len = cur->cmdlen;
		if(lf < len)
			len = lf;
		if(strncasecmp(e->_full_cmd, cur->_full_cmd, len) < 0) {
			SPD_RWLIST_INSERT_BEFORE_CURRENT(e, list);
			break;
		}
			
	}
	SPD_RWLIST_TRAVERSE_SAFE_END;

	if(!cur)
		SPD_RWLIST_INSERT_TAIL(&spd_cli_entrys, e, list);

	return 0;
	
	done:
		SPD_RWLIST_UNLOCK(&spd_cli_entrys);

		return ret;
}

int spd_cli_register(struct spd_cli_entry * e)
{
	return __spd_cli_register(e);
}

int spd_cli_unregister(struct spd_cli_entry *e)
{
	return __spd_cli_unregister(e);
}

int spd_cli_register_multiple(struct spd_cli_entry * e, int len)
{
	int i, res = 0;

	for(i = 0; i < len; i++)
		res |= spd_cli_register(e + i);

	return res;
}

int spd_cli_unregister_multiple(struct spd_cli_entry * e, int len)
{
	int i, res = 0;

	for(i = 0; i < len; i++)
		res |=spd_cli_unregister(e + i);

	return res;
}

static char *parse_args(const char *s, int *argc, const char *argv[], int max, int *trailingwhitespace)
{
	char *duplicate, *cur;
	int x = 0;
	int quoted = 0;
	int escaped = 0;
	int whitespace = 1;
	int dummy = 0;

	if (trailingwhitespace == NULL)
		trailingwhitespace = &dummy;
	*trailingwhitespace = 0;
	if (s == NULL)	/* invalid, though! */
		return NULL;
	/* make a copy to store the parsed string */
	if (!(duplicate = spd_strdup(s)))
		return NULL;

	cur = duplicate;
	/* scan the original string copying into cur when needed */
	for (; *s ; s++) {
		if (x >= max - 1) {
			spd_log(LOG_WARNING, "Too many arguments, truncating at %s\n", s);
			break;
		}
		if (*s == '"' && !escaped) {
			quoted = !quoted;
			if (quoted && whitespace) {
				/* start a quoted string from previous whitespace: new argument */
				argv[x++] = cur;
				whitespace = 0;
			}
		} else if ((*s == ' ' || *s == '\t') && !(quoted || escaped)) {
			/* If we are not already in whitespace, and not in a quoted string or
			   processing an escape sequence, and just entered whitespace, then
			   finalize the previous argument and remember that we are in whitespace
			*/
			if (!whitespace) {
				*cur++ = '\0';
				whitespace = 1;
			}
		} else if (*s == '\\' && !escaped) {
			escaped = 1;
		} else {
			if (whitespace) {
				/* we leave whitespace, and are not quoted. So it's a new argument */
				argv[x++] = cur;
				whitespace = 0;
			}
			*cur++ = *s;
			escaped = 0;
		}
	}
	/* Null terminate */
	*cur++ = '\0';
	/* XXX put a NULL in the last argument, because some functions that take
	 * the array may want a null-terminated array.
	 * argc still reflects the number of non-NULL entries.
	 */
	argv[x] = NULL;
	*argc = x;
	*trailingwhitespace = whitespace;
	return duplicate;
}

int spd_cli_generatornummatches(const char *text, const char *word)
{
	int matches = 0, i = 0;
	char *buf = NULL, *oldbuf = NULL;

	while ((buf = spd_cli_generator(text, word, i++))) {
		if (!oldbuf || strcmp(buf,oldbuf))
			matches++;
		if (oldbuf)
			spd_safe_free(oldbuf);
		oldbuf = buf;
	}
	if (oldbuf)
		spd_safe_free(oldbuf);
	return matches;
}

char **spd_cli_completion_matches(const char *text, const char *word)
{
	char **match_list = NULL, *retstr, *prevstr;
	size_t match_list_len, max_equal, which, i;
	int matches = 0;

	/* leave entry 0 free for the longest common substring */
	match_list_len = 1;
	while ((retstr = spd_cli_generator(text, word, matches)) != NULL) {
		if (matches + 1 >= match_list_len) {
			match_list_len <<= 1;
			if (!(match_list = spd_realloc(match_list, match_list_len * sizeof(*match_list))))
				return NULL;
		}
		match_list[++matches] = retstr;
	}

	if (!match_list)
		return match_list; /* NULL */

	/* Find the longest substring that is common to all results
	 * (it is a candidate for completion), and store a copy in entry 0.
	 */
	prevstr = match_list[1];
	max_equal = strlen(prevstr);
	for (which = 2; which <= matches; which++) {
		for (i = 0; i < max_equal && toupper(prevstr[i]) == toupper(match_list[which][i]); i++)
			continue;
		max_equal = i;
	}

	if (!(retstr = spd_malloc(max_equal + 1)))
		return NULL;
	
	spd_copy_string(retstr, match_list[1], max_equal + 1);
	match_list[0] = retstr;

	/* ensure that the array is NULL terminated */
	if (matches + 1 >= match_list_len) {
		if (!(match_list = spd_realloc(match_list, (match_list_len + 1) * sizeof(*match_list))))
			return NULL;
	}
	match_list[matches + 1] = NULL;

	return match_list;
}

/*! \brief returns true if there are more words to match */
static int more_words (const char * const *dst)
{
	int i;
	for (i = 0; dst[i]; i++) {
		if (dst[i][0] != '[')
			return -1;
	}
	return 0;
}

/*
 * generate the entry at position 'state'
 */
static char *__spd_cli_generator(const char *text, const char *word, int state, int lock)
{
	const char *argv[SPD_MAX_ARGS];
	struct spd_cli_entry *e = NULL;
	int x = 0, argindex, matchlen;
	int matchnum=0;
	char *ret = NULL;
	char matchstr[80] = "";
	int tws = 0;
	/* Split the argument into an array of words */
	char *duplicate = parse_args(text, &x, argv, ARRAY_LEN(argv), &tws);

	if (!duplicate)	/* malloc error */
		return NULL;

	/* Compute the index of the last argument (could be an empty string) */
	argindex = (!spd_strlen_zero(word) && x>0) ? x-1 : x;

	/* rebuild the command, ignore terminating white space and flatten space */
	spd_str_join(matchstr, sizeof(matchstr)-1, argv);
	matchlen = strlen(matchstr);
	if (tws) {
		strcat(matchstr, " "); /* XXX */
		if (matchlen)
			matchlen++;
	}
	if (lock)
		SPD_RWLIST_RDLOCK(&spd_cli_entrys);
	while ( (e = cli_next(e)) ) {
		/* XXX repeated code */
		int src = 0, dst = 0, n = 0;

		if (e->command[0] == '_')
			continue;

		/*
		 * Try to match words, up to and excluding the last word, which
		 * is either a blank or something that we want to extend.
		 */
		for (;src < argindex; dst++, src += n) {
			n = word_match(argv[src], e->cmda[dst]);
			if (n < 0)
				break;
		}

		if (src != argindex && more_words(e->cmda + dst))	/* not a match */
			continue;
		ret = is_prefix(argv[src], e->cmda[dst], state - matchnum, &n);
		matchnum += n;	/* this many matches here */
		if (ret) {
			/*
			 * argv[src] is a valid prefix of the next word in this
			 * command. If this is also the correct entry, return it.
			 */
			if (matchnum > state)
				break;
			spd_safe_free(ret);
			ret = NULL;
		} else if (spd_strlen_zero(e->cmda[dst])) {
			/*
			 * This entry is a prefix of the command string entered
			 * (only one entry in the list should have this property).
			 * Run the generator if one is available. In any case we are done.
			 */
			if (e->handler) {	/* new style command */
				struct spd_cli_arg a = {
					.line = matchstr, .word = word,
					.pos = argindex,
					.n = state - matchnum,
					.argv = argv,
					.argc = x};
				ret = e->handler(e, CLI_COMPLETE, &a);
			}
			if (ret)
				break;
		}
	}
	if (lock)
		SPD_RWLIST_UNLOCK(&spd_cli_entrys);
	spd_safe_free(duplicate);
	
	return ret;
}
char *spd_cli_generator(const char *text, const char *word, int state)
{
	return __spd_cli_generator(text, word, state, 1);
}

int spd_cli_command_full(int uid, int gid, int fd, const char *s)
{
	const char *args[SPD_MAX_ARGS + 1];
	struct spd_cli_entry *e;
	int x;
	char *duplicate = parse_args(s, &x, args + 1, SPD_MAX_ARGS, NULL);
	char tmp[SPD_MAX_ARGS + 1];
	char *retval = NULL;
	struct spd_cli_arg a = {
		.fd = fd, .argc = x, .argv = args+1 };

	if (duplicate == NULL)
		return -1;

	if (x < 1)	/* We need at least one entry, otherwise ignore */
		goto done;

	SPD_RWLIST_RDLOCK(&spd_cli_entrys);
	e = find_cli(args + 1, 0);
	if (e)
		spd_atomic_fetchadd_int(&e->inuse, 1);
	SPD_RWLIST_UNLOCK(&spd_cli_entrys);
	if (e == NULL) {
		spd_cli(fd, "No such command '%s' (type 'core show help %s' for other possible commands)\n", s, find_best(args + 1));
		goto done;
	}

	spd_str_join(tmp, sizeof(tmp), args + 1);
	/* Check if the user has rights to run this command. */
	/*if (!cli_has_permissions(uid, gid, tmp)) {
		spd_cli(fd, "You don't have permissions to run '%s' command\n", tmp);
		spd_safe_free(duplicate);
		return 0;
	} */

	/*
	 * Within the handler, argv[-1] contains a pointer to the spd_cli_entry.
	 * Remember that the array returned by parse_args is NULL-terminated.
	 */
	args[0] = (char *)e;

	retval = e->handler(e, CLI_HANDLER, &a);

	if (retval == CLI_SHOWUSAGE) {
		spd_cli(fd, "%s", F_OR(e->usage, "Invalid usage, but no usage information available.\n"));
	} else {
		if (retval == CLI_FAILURE)
			spd_cli(fd, "Command '%s' failed.\n", s);
	}
	spd_atomic_fetchadd_int(&e->inuse, -1);
done:
	spd_safe_free(duplicate);
	return 0;
}

int spd_cli_command_multiple_full(int uid, int gid, int fd, size_t size, const char *s)
{
	char cmd[512];
	int x, y = 0, count = 0;

	for (x = 0; x < size; x++) {
		cmd[y] = s[x];
		y++;
		if (s[x] == '\0') {
			spd_cli_command_full(uid, gid, fd, cmd);
			y = 0;
			count++;
		}
	}
	return count;
}

