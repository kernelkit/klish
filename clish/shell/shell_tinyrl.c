/*
 * shell_tinyrl.c
 *
 * This is a specialisation of the tinyrl_t class which maps the readline
 * functionality to the CLISH environment.
 */
#include "private.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "tinyrl/tinyrl.h"
#include "tinyrl/vt100.h"
#include "tinyrl/history.h"

#include "lub/string.h"

/*-------------------------------------------------------- */
static void clish_shell_renew_prompt(clish_shell_t *this)
{
	clish_context_t prompt_context;
	char *prompt = NULL;
	const clish_view_t *view;
	char *str = NULL;

	/* Create appropriate context */
	clish_context_init(&prompt_context, this);

	/* Obtain the prompt */
	view = clish_shell__get_view(this);
	assert(view);
	lub_string_cat(&str, "${_PROMPT_PREFIX}");
	lub_string_cat(&str, clish_view__get_prompt(view));
	lub_string_cat(&str, "${_PROMPT_SUFFIX}");
	prompt = clish_shell_expand(str, SHELL_VAR_NONE, &prompt_context);
	assert(prompt);
	lub_string_free(str);
	tinyrl__set_prompt(this->tinyrl, prompt);
	lub_string_free(prompt);
}

/*-------------------------------------------------------- */
static bool_t clish_shell_tinyrl_key_help(tinyrl_t *this, int key)
{
	bool_t result = BOOL_TRUE;

	if (tinyrl_is_quoting(this) || tinyrl_get_ins_flag(this)) {
		/* if we are in the middle of a quote then simply enter a space */
		result = tinyrl_insert_text(this, "?");
	} else {
		/* get the context */
		clish_context_t *context = tinyrl__get_context(this);
		clish_shell_t *shell = clish_context__get_shell(context);
		tinyrl_crlf(this);
		clish_shell_help(shell, tinyrl__get_line(this));
		tinyrl_crlf(this);
		tinyrl_reset_line_state(this);
	}
	/* keep the compiler happy */
	key = key;

	return result;
}

/*lint +e818 */
/*-------------------------------------------------------- */
/*
 * Expand the current line with any history substitutions
 */
static clish_pargv_status_e clish_shell_tinyrl_expand(tinyrl_t *this)
{
	clish_pargv_status_e status = CLISH_LINE_OK;
#if 0
	int rtn;
	char *buffer;

	/* first of all perform any history substitutions */
	rtn = tinyrl_history_expand(tinyrl__get_history(this),
		tinyrl__get_line(this), &buffer);

	switch (rtn) {
	case -1:
		/* error in expansion */
		status = CLISH_BAD_HISTORY;
		break;
	case 0:
		/*no expansion */
		break;
	case 1:
		/* expansion occured correctly */
		tinyrl_replace_line(this, buffer, 1);
		break;
	case 2:
		/* just display line */
		tinyrl_printf(this, "\n%s", buffer);
		free(buffer);
		buffer = NULL;
		break;
	default:
		break;
	}
	free(buffer);

#endif
	this = this;
	return status;
}

/*-------------------------------------------------------- */
/*
 * This is a CLISH specific completion function.
 * If the current prefix is not a recognised prefix then
 * an error is flagged.
 * If it is a recognisable prefix then possible completions are displayed
 * or a unique completion is inserted.
 */
static tinyrl_match_e clish_shell_tinyrl_complete(tinyrl_t *this)
{
	tinyrl_match_e status;

	/* first of all perform any history expansion */
	(void)clish_shell_tinyrl_expand(this);
	/* perform normal completion */
	status = tinyrl_complete(this);
	switch (status) {
	case TINYRL_NO_MATCH:
		if (BOOL_FALSE == tinyrl_is_completion_error_over(this)) {
			/* The user hasn't even entered a valid prefix! */
/*			tinyrl_crlf(this);
			clish_shell_help(context->shell,
				tinyrl__get_line(this));
			tinyrl_crlf(this);
			tinyrl_reset_line_state(this);
*/		}
		break;
	default:
		/* the default completion function will have prompted for completions as
		 * necessary
		 */
		break;
	}
	return status;
}

/*--------------------------------------------------------- */
static bool_t clish_shell_tinyrl_key_space(tinyrl_t *this, int key)
{
	bool_t result = BOOL_FALSE;
	tinyrl_match_e status;
	clish_context_t *context = tinyrl__get_context(this);
	clish_shell_t *shell = clish_context__get_shell(context);
	const char *line = tinyrl__get_line(this);
	clish_pargv_status_e arg_status;
	const clish_command_t *cmd = NULL;
	clish_pargv_t *pargv = NULL;

	/* ignore space at the begining of the line, don't display commands */
	if (tinyrl_is_empty(this))
		return BOOL_TRUE;
	
	/* if we are in the middle of a quote then simply enter a space */
	/* If it's not a quotation */
	if (!tinyrl_is_quoting(this)) {

		/* Find out if current line is legal. It can be
		 * fully completed or partially completed.
		 */
		arg_status = clish_shell_parse(shell, line, &cmd, &pargv);
		if (pargv)
			clish_pargv_delete(pargv);
		switch (arg_status) {
		case CLISH_LINE_OK:
		case CLISH_LINE_PARTIAL:
			if (' ' != line[strlen(line) - 1])
				result = BOOL_TRUE;
			break;
		default:
			break;
		}

		/* If current line is illegal try to make auto-comletion. */
		if (!result) {
			/* perform word completion */
			status = clish_shell_tinyrl_complete(this);
			switch (status) {
			case TINYRL_NO_MATCH:
			case TINYRL_AMBIGUOUS:
				/* ambiguous result signal an issue */
				break;
			case TINYRL_COMPLETED_AMBIGUOUS:
				/* perform word completion again in case we just did case
				   modification the first time */
				clish_shell_tinyrl_complete(this);
				//status = clish_shell_tinyrl_complete(this);
				//if (status == TINYRL_MATCH_WITH_EXTENSIONS) {
					/* all is well with the world just enter a space */
				//	result = BOOL_TRUE;
				//}
				break;
			case TINYRL_MATCH:
			case TINYRL_MATCH_WITH_EXTENSIONS:
			case TINYRL_COMPLETED_MATCH:
				/* all is well with the world just enter a space */
				//result = BOOL_TRUE;
				break;
			}
		}
	}
// Permit to press space in any cases
//	if (result)
		result = tinyrl_insert_text(this, " ");
	/* keep compiler happy */
	key = key;

	return result;
}

/*-------------------------------------------------------- */
static bool_t clish_shell_tinyrl_key_enter(tinyrl_t *this, int key)
{
	clish_context_t *context = tinyrl__get_context(this);
	clish_shell_t *shell = clish_context__get_shell(context);
	const char *line = tinyrl__get_line(this);
	bool_t result = BOOL_FALSE;
	char *errmsg = NULL;
	tinyrl_history_t *history = NULL;
	clish_pargv_status_e arg_status = CLISH_LINE_OK;

	// Increment line counter
	if (shell->current_file)
		shell->current_file->line++;

	// Nothing to pass simply move down the screen
	if (!*line) {
		tinyrl_multi_crlf(this);
		tinyrl_done(this);
		return BOOL_TRUE;
	}

	// Workaround on ugly history.
	// The line can be the pointer to history entry. Now we must fix it
	// and copy string to real buffer.
	tinyrl_changed_line(this);
	line = tinyrl__get_line(this);

	// Parse line
	arg_status = clish_shell_parse(shell,
		line, &context->cmd, &context->pargv);

	// If command is incompleted then try to autocomplete it
	if (arg_status != CLISH_LINE_OK) {
		tinyrl_match_e status = clish_shell_tinyrl_complete(this);

		switch (status) {
		case TINYRL_MATCH:
		case TINYRL_MATCH_WITH_EXTENSIONS:
		case TINYRL_COMPLETED_MATCH:
			// Re-fetch the line as it may have changed
			// due to auto-completion
			line = tinyrl__get_line(this);
			// Try to parse line another time after autocompletion
			arg_status = clish_shell_parse(shell,
				line, &context->cmd, &context->pargv);
			// We have had a match but it is not a command
			// so add a space so as not to confuse the user
			if (!context->cmd)
				result = tinyrl_insert_text(this, " ");
			break;
		default:
			break;
		}
	}

	// Add any not-null line to history
	if (tinyrl__get_isatty(this)) {
		history = tinyrl__get_history(this);
		tinyrl_history_add(history, tinyrl__get_line(this));
	}
	tinyrl_multi_crlf(this);

	if (context->cmd) {
		switch (arg_status) {
		case CLISH_LINE_OK:
			result = BOOL_TRUE;
			break;
		case CLISH_BAD_HISTORY:
			errmsg = "Bad history entry";
			break;
		case CLISH_BAD_CMD:
			errmsg = "Illegal command line";
			break;
		case CLISH_BAD_PARAM:
			errmsg = "Illegal parameter";
			break;
		case CLISH_LINE_PARTIAL:
			errmsg = "Incompleted command";
			break;
		default:
			errmsg = "Unknown problem";
			break;
		}
	} else {
		errmsg = "Unknown command";
	}

	// If error then print message
	if (errmsg) {
		if (tinyrl__get_isatty(this) || !shell->current_file) {
			fprintf(stderr, "Syntax error: %s\n", errmsg);
		} else {
			char *fname = "stdin";
			if (shell->current_file->fname)
				fname = shell->current_file->fname;
			fprintf(stderr, "Syntax error on line %s:%u \"%s\": "
			"%s\n", fname, shell->current_file->line, line, errmsg);
		}
		// Wrong line must return bad retval for machine oriented proto
		// Let retval=2 means wrong/incompleted command.
		// Note there is some ugly architecture aspect - machine retval
		// and logging are executed within clish_shell_execute()
		// function also. The clish_shell_execute() is for completed
		// commands but this function for wrong/incompleted commands.
		clish_shell_machine_retval(shell, 2);
		/* Call logging callback */
		if (clish_shell__get_log(shell) &&
			clish_shell_check_hook(context, CLISH_SYM_TYPE_LOG)) {
			char *s = NULL;
			lub_string_cat(&s, line);
			lub_string_cat(&s, " [syntax]");
			clish_shell_exec_log(context, s, 2);
			lub_string_free(s);
		}
	}

	tinyrl_done(this);
	key = key; // Happy compiler

	return result;
}

/*-------------------------------------------------------- */
static bool_t clish_shell_tinyrl_hotkey(tinyrl_t *this, int key)
{
	clish_view_t *view;
	const char *cmd = NULL;
	clish_context_t *context = tinyrl__get_context(this);
	clish_shell_t *shell = clish_context__get_shell(context);
	int i;
	char *tmp = NULL;

	i = clish_shell__get_depth(shell);
	while (i >= 0) {
		view = clish_shell__get_pwd_view(shell, i);
		cmd = clish_view_find_hotkey(view, key);
		if (cmd)
			break;
		i--;
	}
	/* Check the global view */
	if (i < 0) {
		view = shell->global;
		cmd = clish_view_find_hotkey(view, key);
	}
	if (!cmd)
		return BOOL_FALSE;

	tmp = clish_shell_expand(cmd, SHELL_VAR_NONE, context);
	tinyrl_replace_line(this, tmp, 0);
	lub_string_free(tmp);
	clish_shell_tinyrl_key_enter(this, 0);

	return BOOL_TRUE;
}

/*-------------------------------------------------------- */
/* This is the completion function provided for CLISH */
tinyrl_completion_func_t clish_shell_tinyrl_completion;
char **clish_shell_tinyrl_completion(tinyrl_t * tinyrl,
	const char *line, unsigned start, unsigned end)
{
	lub_argv_t *matches;
	clish_context_t *context = tinyrl__get_context(tinyrl);
	clish_shell_t *this = clish_context__get_shell(context);
	clish_shell_iterator_t iter;
	const clish_command_t *cmd = NULL;
	char *text;
	char **result = NULL;

	if (tinyrl_is_quoting(tinyrl))
		return result;

	matches = lub_argv_new(NULL, 0);
	text = lub_string_dupn(line, end);

	/* Don't bother to resort to filename completion */
	tinyrl_completion_over(tinyrl);

	/* Search for COMMAND completions */
	clish_shell_iterator_init(&iter, CLISH_NSPACE_COMPLETION);
	while ((cmd = clish_shell_find_next_completion(this, text, &iter)))
		lub_argv_add(matches, clish_command__get_suffix(cmd));

	/* Try and resolve a command */
	cmd = clish_shell_resolve_command(this, text);
	/* Search for PARAM completion */
	if (cmd)
		clish_shell_param_generator(this, matches, cmd, text, start);

	lub_string_free(text);

	/* Matches were found */
	if (lub_argv__get_count(matches) > 0) {
		unsigned i;
		char *subst = lub_string_dup(lub_argv__get_arg(matches, 0));
		/* Find out substitution */
		for (i = 1; i < lub_argv__get_count(matches); i++) {
			char *p = subst;
			const char *match = lub_argv__get_arg(matches, i);
			size_t match_len = strlen(p);
			/* identify the common prefix */
			while ((tolower(*p) == tolower(*match)) && match_len--) {
				p++;
				match++;
			}
			/* Terminate the prefix string */
			*p = '\0';
		}
		result = lub_argv__get_argv(matches, subst);
		lub_string_free(subst);
	}
	lub_argv_delete(matches);

	return result;
}

/*-------------------------------------------------------- */
static void clish_shell_tinyrl_init(tinyrl_t * this)
{
	bool_t status;
	/* bind the '?' key to the help function */
	status = tinyrl_bind_key(this, '?', clish_shell_tinyrl_key_help);
	assert(status);

	/* bind the <RET> key to the help function */
	status = tinyrl_bind_key(this, '\r', clish_shell_tinyrl_key_enter);
	assert(status);
	status = tinyrl_bind_key(this, '\n', clish_shell_tinyrl_key_enter);
	assert(status);

	/* bind the <SPACE> key to auto-complete if necessary */
	status = tinyrl_bind_key(this, ' ', clish_shell_tinyrl_key_space);
	assert(status);

	/* Set external hotkey callback */
	tinyrl__set_hotkey_fn(this, clish_shell_tinyrl_hotkey);

	/* Assign timeout callback */
	tinyrl__set_timeout_fn(this, clish_shell_timeout_fn);
	/* Assign keypress callback */
	tinyrl__set_keypress_fn(this, clish_shell_keypress_fn);
}

/*-------------------------------------------------------- */
/*
 * Create an instance of the specialised class
 */
tinyrl_t *clish_shell_tinyrl_new(FILE * istream,
	FILE * ostream, unsigned stifle)
{
	/* call the parent constructor */
	tinyrl_t *this = tinyrl_new(istream,
		ostream, stifle, clish_shell_tinyrl_completion);
	/* now call our own constructor */
	if (this)
		clish_shell_tinyrl_init(this);
	return this;
}

/*-------------------------------------------------------- */
void clish_shell_tinyrl_fini(tinyrl_t * this)
{
	/* nothing to do... yet */
	this = this;
}

/*-------------------------------------------------------- */
void clish_shell_tinyrl_delete(tinyrl_t * this)
{
	/* call our destructor */
	clish_shell_tinyrl_fini(this);
	/* and call the parent destructor */
	tinyrl_delete(this);
}

/*-------------------------------------------------------- */
static int clish_shell_execline(clish_shell_t *this, const char *line, char **out)
{
	char *str;
	clish_context_t context;
	int lerror = 0;

	assert(this);
	this->state = SHELL_STATE_OK;
	if (!line && !tinyrl__get_istream(this->tinyrl)) {
		this->state = SHELL_STATE_SYSTEM_ERROR;
		return -1;
	}

	/* Renew prompt */
	clish_shell_renew_prompt(this);

	/* Set up the context for tinyrl */
	clish_context_init(&context, this);

	/* Push the specified line or interactive line */
	if (line)
		str = tinyrl_forceline(this->tinyrl, &context, line);
	else
		str = tinyrl_readline(this->tinyrl, &context);
	lerror = errno;
	if (!str) {
		switch (lerror) {
		case ENOENT:
			this->state = SHELL_STATE_EOF;
			break;
		case ENOEXEC:
			this->state = SHELL_STATE_SYNTAX_ERROR;
			break;
		default:
			this->state = SHELL_STATE_SYSTEM_ERROR;
			break;
		};
		return -1;
	}
	lub_string_free(str);

	/* Execute the provided command */
	if (context.cmd && context.pargv) {
		int res;
		if ((res = clish_shell_execute(&context, out))) {
			this->state = SHELL_STATE_SCRIPT_ERROR;
			if (context.pargv)
				clish_pargv_delete(context.pargv);
			return res;
		}
	}

	if (context.pargv)
		clish_pargv_delete(context.pargv);

	return 0;
}

/*-------------------------------------------------------- */
int clish_shell_forceline(clish_shell_t *this, const char *line, char **out)
{
	return clish_shell_execline(this, line, out);
}

/*-------------------------------------------------------- */
int clish_shell_readline(clish_shell_t *this, char **out)
{
	return clish_shell_execline(this, NULL, out);
}

/*-------------------------------------------------------- */
FILE * clish_shell__get_istream(const clish_shell_t * this)
{
	return tinyrl__get_istream(this->tinyrl);
}

/*-------------------------------------------------------- */
FILE * clish_shell__get_ostream(const clish_shell_t * this)
{
	return tinyrl__get_ostream(this->tinyrl);
}


/*-------------------------------------------------------- */
bool_t clish_shell__get_utf8(const clish_shell_t * this)
{
	assert(this);
	return tinyrl__get_utf8(this->tinyrl);
}

/*-------------------------------------------------------- */
void clish_shell__set_utf8(clish_shell_t * this, bool_t utf8)
{
	assert(this);
	tinyrl__set_utf8(this->tinyrl, utf8);
}

/*--------------------------------------------------------- */
tinyrl_t *clish_shell__get_tinyrl(const clish_shell_t * this)
{
	return this->tinyrl;
}

/*----------------------------------------------------------*/
int clish_shell__save_history(const clish_shell_t *this, const char *fname)
{
	return tinyrl__save_history(this->tinyrl, fname);
}

/*----------------------------------------------------------*/
int clish_shell__restore_history(clish_shell_t *this, const char *fname)
{
	return tinyrl__restore_history(this->tinyrl, fname);
}

/*----------------------------------------------------------*/
void clish_shell__stifle_history(clish_shell_t *this, unsigned int stifle)
{
	tinyrl__stifle_history(this->tinyrl, stifle);
}

/*----------------------------------------------------------*/
void clish_shell__disable_echo(clish_shell_t *this)
{
	tinyrl_disable_echo(this->tinyrl, '\0');
}

CLISH_SET(shell, unsigned int, idle_timeout);
CLISH_SET(shell, bool_t, interactive);
CLISH_GET(shell, bool_t, interactive);
