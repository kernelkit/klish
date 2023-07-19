#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include <faux/faux.h>
#include <faux/str.h>

#include "private.h"


static int is_blank(const char *str, off_t pos, int utf8)
{
	if (utf8)
		return iswspace(str[pos]);
	return isspace(str[pos]);
}

static off_t move_left(const char *str, off_t pos, int utf8)
{
	if (utf8)
		return utf8_move_left(str, pos);

	return str && pos > 0 ? pos - 1 : 0;
}

static off_t move_right(const char *str, off_t pos, int utf8)
{
	if (utf8)
		return utf8_move_right(str, pos);

	return str && (size_t)pos < strlen(str) ? pos + 1 : pos;
}

bool_t tinyrl_key_default(tinyrl_t *tinyrl, unsigned char key)
{
	if (key > 31) {
		// Inject new char to the line
		tinyrl_line_insert(tinyrl, (const char *)(&key), 1);
	} else {
		// Call the external hotkey analyzer
		if (tinyrl->hotkey_fn)
			tinyrl->hotkey_fn(tinyrl, key);
	}

	return BOOL_TRUE;
}


bool_t tinyrl_key_interrupt(tinyrl_t *tinyrl, unsigned char key)
{
	tinyrl_multi_crlf(tinyrl);
	tinyrl_reset_line_state(tinyrl);
	tinyrl_reset_line(tinyrl);
	tinyrl_reset_hist_pos(tinyrl);

	// Happy compiler
	key = key;

	return BOOL_TRUE;
}


bool_t tinyrl_key_start_of_line(tinyrl_t *tinyrl, unsigned char key)
{
	// Set current position to the start of the line
	tinyrl->line.pos = 0;

	// Happy compiler
	key = key;

	return BOOL_TRUE;
}


bool_t tinyrl_key_end_of_line(tinyrl_t *tinyrl, unsigned char key)
{
	// Set current position to the end of the line
	tinyrl->line.pos = tinyrl->line.len;

	// Happy compiler
	key = key;

	return BOOL_TRUE;
}


bool_t tinyrl_key_kill(tinyrl_t *tinyrl, unsigned char key)
{
	size_t len;

	(void)key;

	// release any old kill string 
	faux_free(tinyrl->kill_string);

	// store the killed string 
	tinyrl->kill_string = strdup(&tinyrl->line.str[tinyrl->line.pos]);

	// delete the text to the end of the line
	len = strlen(&tinyrl->line.str[tinyrl->line.pos]);
	tinyrl_line_delete(tinyrl, tinyrl->line.pos, len);

	return BOOL_TRUE;
}


bool_t tinyrl_key_yank(tinyrl_t *tinyrl, unsigned char key)
{
	(void)key;

	if (tinyrl->kill_string) {
		// insert the kill string at the current insertion point 
		return tinyrl_line_insert(tinyrl, tinyrl->kill_string, strlen(tinyrl->kill_string));
	}

	return BOOL_TRUE;
}


bool_t tinyrl_key_yank_arg(tinyrl_t *tinyrl, unsigned char key)
{
	const char *prev, *arg = NULL;
	off_t pos;

	(void)key;

	prev = hist_prev_line(tinyrl->hist);
	if (!prev)
		return BOOL_TRUE;

	pos = strlen(prev);
	while (pos > 0) {
		pos = move_left(prev, pos, tinyrl->utf8);
		if (is_blank(prev, pos, tinyrl->utf8)) {
			if (arg)
				break; // complete word
			continue;
		}
		arg = &prev[pos]; // candidate
	}

	return tinyrl_line_insert(tinyrl, arg, strlen(arg));
}


// Default handler for crlf
bool_t tinyrl_key_crlf(tinyrl_t *tinyrl, unsigned char key)
{

	tinyrl_multi_crlf(tinyrl);
	tinyrl_reset_line_state(tinyrl);
	tinyrl_reset_line(tinyrl);
	tinyrl_reset_hist_pos(tinyrl);

	key = key; // Happy compiler

	return BOOL_TRUE;
}


bool_t tinyrl_key_comment(tinyrl_t *tinyrl, unsigned char key)
{
	char buf[2] = { key, 0 };

	tinyrl->line.pos = 0;
	tinyrl_line_insert(tinyrl, buf, 1);
	tinyrl_redisplay(tinyrl);

	return tinyrl->handlers[KEY_CR](tinyrl, KEY_CR);
}


bool_t tinyrl_key_up(tinyrl_t *tinyrl, unsigned char key)
{
	const char *str = NULL;

	str = hist_pos(tinyrl->hist);
	// Up key is pressed for the first time and current line is new typed one
	if (!str) {
		hist_add(tinyrl->hist, tinyrl->line.str, BOOL_TRUE); // Temp entry
		hist_pos_up(tinyrl->hist); // Skip newly added temp entry
	}
	hist_pos_up(tinyrl->hist);
	str = hist_pos(tinyrl->hist);
	// Empty history
	if (!str)
		return BOOL_TRUE;
	tinyrl_line_replace(tinyrl, str);

	key = key; // Happy compiler

	return BOOL_TRUE;
}


bool_t tinyrl_key_down(tinyrl_t *tinyrl, unsigned char key)
{
	const char *str = NULL;

	hist_pos_down(tinyrl->hist);
	str = hist_pos(tinyrl->hist);
	// Empty history
	if (!str)
		return BOOL_TRUE;
	tinyrl_line_replace(tinyrl, str);

	key = key; // Happy compiler

	return BOOL_TRUE;
}


/*
 * Search history for a match.  Input keys form an isearch substring
 * where the first match is displayed, along with the substring.  An
 * empty match shows the last match, any new input text that matches
 * the same line keeps that same line.  To loop through all matching
 * lines, press Ctrl-R.
 */
static bool_t tinyrl_hist_search(tinyrl_t *tinyrl, char key)
{
	char prompt[strlen(tinyrl->isearch) + 30];
	bool_t rc = BOOL_TRUE;
	size_t len, room;

	/* Any special key execpt Ctrl-R and Ctrl-L exit i-search */
	if (key < 32) {
		if (key == KEY_FF || key == KEY_DC2)
			goto refresh;

		rc = tinyrl->isearch_cont = BOOL_FALSE;
		tinyrl_set_prompt(tinyrl, tinyrl->saved_prompt);
		goto display;
	}

	len = strlen(tinyrl->isearch);
	room = sizeof(tinyrl->isearch) - len - 1;

	if (key == KEY_DEL)
		tinyrl->isearch[len > 0 ? len - 1 : 0] = 0;
	else if (room)
		tinyrl->isearch[len] = key;
	else
		return BOOL_TRUE;
refresh:
	if (strlen(tinyrl->isearch)) {
		const char *match;

		match = hist_search_current(tinyrl->hist, tinyrl->isearch);
		if (!match || key == KEY_DEL || key == KEY_DC2)
			match = hist_search_substr(tinyrl->hist, tinyrl->isearch, key == KEY_DEL);
		if (match)
			tinyrl_line_replace(tinyrl, match);
	} else {
		hist_search_reset(tinyrl->hist);
		tinyrl_reset_line(tinyrl);
	}

	snprintf(prompt, sizeof(prompt), "(reverse i-search)`%s': ", tinyrl->isearch);
	tinyrl_set_prompt(tinyrl, prompt);

display:
	tinyrl_reset_line_state(tinyrl);
	vt100_printf(tinyrl->term, "\r");
	vt100_erase_line(tinyrl->term);
	tinyrl_redisplay(tinyrl);

	return rc;
}


/*
 * Start reversed interactive (i-search) when user taps Ctrl-R, any text
 * on the line at that time will be used as a search key in history.  We
 * save the previous prompt and restore it when the user exits i-search.
 */
bool_t tinyrl_key_isearch(tinyrl_t *tinyrl, unsigned char key)
{
	if (!tinyrl->isearch_cont) {
		tinyrl->isearch_cont = BOOL_TRUE;
		hist_search_reset(tinyrl->hist);

		memset(tinyrl->isearch, 0, sizeof(tinyrl->isearch));
		strncpy(tinyrl->isearch, tinyrl->line.str, sizeof(tinyrl->isearch) - 1);

		tinyrl->saved_prompt = faux_str_dup(tinyrl->prompt);
	}

	return tinyrl_hist_search(tinyrl, key);
}


bool_t tinyrl_key_left(tinyrl_t *tinyrl, unsigned char key)
{
	if (tinyrl->line.pos == 0)
		return BOOL_TRUE;

	if (tinyrl->utf8)
		tinyrl->line.pos = utf8_move_left(tinyrl->line.str, tinyrl->line.pos);
	else
		tinyrl->line.pos--;

	// Happy compiler
	key = key;

	return BOOL_TRUE;
}


bool_t tinyrl_key_right(tinyrl_t *tinyrl, unsigned char key)
{
	if (tinyrl->line.pos == tinyrl->line.len)
		return BOOL_TRUE;

	if (tinyrl->utf8)
		tinyrl->line.pos = utf8_move_right(tinyrl->line.str, tinyrl->line.pos);
	else
		tinyrl->line.pos++;

	// Happy compiler
	key = key;

	return BOOL_TRUE;
}


bool_t tinyrl_key_backspace(tinyrl_t *tinyrl, unsigned char key)
{
	if (tinyrl->line.pos == 0)
		return BOOL_TRUE;

	if (tinyrl->utf8) {
		off_t new_pos = 0;
		new_pos = utf8_move_left(tinyrl->line.str, tinyrl->line.pos);
		tinyrl_line_delete(tinyrl, new_pos, tinyrl->line.pos - new_pos);
	} else {
		tinyrl_line_delete(tinyrl, tinyrl->line.pos - 1, 1);
	}

	// Happy compiler
	key = key;

	return BOOL_TRUE;
}


bool_t tinyrl_key_delete(tinyrl_t *tinyrl, unsigned char key)
{
	if (tinyrl->line.pos == tinyrl->line.len)
		return BOOL_TRUE;

	if (tinyrl->utf8) {
		off_t new_pos = 0;
		new_pos = utf8_move_right(tinyrl->line.str, tinyrl->line.pos);
		tinyrl_line_delete(tinyrl, tinyrl->line.pos, new_pos - tinyrl->line.pos);
	} else {
		tinyrl_line_delete(tinyrl, tinyrl->line.pos, 1);
	}

	// Happy compiler
	key = key;

	return BOOL_TRUE;
}

static int is_space(tinyrl_t *tinyrl)
{
	return is_blank(tinyrl->line.str, tinyrl->line.pos, tinyrl->utf8);
}

static int is_prev_space(tinyrl_t *tinyrl)
{
	off_t pos = move_left(tinyrl->line.str, tinyrl->line.pos, tinyrl->utf8);

	return is_blank(tinyrl->line.str, pos, tinyrl->utf8);
}

static int is_next_space(tinyrl_t *tinyrl)
{
	off_t pos = move_right(tinyrl->line.str, tinyrl->line.pos, tinyrl->utf8);

	return is_blank(tinyrl->line.str, pos, tinyrl->utf8);
}

bool_t tinyrl_key_backword(tinyrl_t *tinyrl, unsigned char key)
{
	(void)key;

	// remove current whitespace before cursor
	while (tinyrl->line.pos > 0 && is_prev_space(tinyrl))
		tinyrl_key_backspace(tinyrl, KEY_BS);

	// delete word before cusor
	while (tinyrl->line.pos > 0 && !is_prev_space(tinyrl))
		tinyrl_key_backspace(tinyrl, KEY_BS);

	return BOOL_TRUE;
}

bool_t tinyrl_key_delword(tinyrl_t *tinyrl, unsigned char key)
{
	(void)key;

	// remove current whitespace at cursor
	while (tinyrl->line.pos < tinyrl->line.len && is_space(tinyrl))
		tinyrl_key_delete(tinyrl, KEY_DEL);

	// delete word at cusor
	while (tinyrl->line.pos < tinyrl->line.len && !is_space(tinyrl))
		tinyrl_key_delete(tinyrl, KEY_DEL);

	return BOOL_TRUE;
}

bool_t tinyrl_key_left_word(tinyrl_t *tinyrl, unsigned char key)
{
	(void)key;

	while (tinyrl->line.pos > 0 && is_prev_space(tinyrl))
		tinyrl_key_left(tinyrl, key);

	while (tinyrl->line.pos > 0 && !is_prev_space(tinyrl))
		tinyrl_key_left(tinyrl, key);

	return BOOL_TRUE;
}

bool_t tinyrl_key_right_word(tinyrl_t *tinyrl, unsigned char key)
{
	int adjust = 0;

	(void)key;

	while (tinyrl->line.pos < tinyrl->line.len && !is_next_space(tinyrl))
		tinyrl_key_right(tinyrl, key);

	while (tinyrl->line.pos < tinyrl->line.len && is_next_space(tinyrl)) {
		adjust = 1;
		tinyrl_key_right(tinyrl, key);
	}

	if (adjust)
		tinyrl_key_right(tinyrl, key);

	return BOOL_TRUE;
}


bool_t tinyrl_key_clear_screen(tinyrl_t *tinyrl, unsigned char key)
{
	vt100_clear_screen(tinyrl->term);
	vt100_cursor_home(tinyrl->term);
	tinyrl_reset_line_state(tinyrl);

	key = key; // Happy compiler

	return BOOL_TRUE;
}


bool_t tinyrl_key_erase_line(tinyrl_t *tinyrl, unsigned char key)
{
	size_t len;

	(void)key;

	// release any old kill string 
	faux_free(tinyrl->kill_string);

	if (!tinyrl->line.pos) {
		tinyrl->kill_string = NULL;
		return BOOL_TRUE;
	}

	// store the killed string
	len = strlen(tinyrl->line.str) + 1;
	tinyrl->kill_string = malloc(len);
	memcpy(tinyrl->kill_string, tinyrl->line.str, len);

	// delete the text from the start of the line 
	tinyrl_reset_line(tinyrl);

	return BOOL_TRUE;
}


bool_t tinyrl_key_tab(tinyrl_t *tinyrl, unsigned char key)
{
	bool_t result = BOOL_FALSE;
/*
	tinyrl_match_e status = tinyrl_complete_with_extensions(tinyrl);

	switch (status) {
	case TINYRL_COMPLETED_MATCH:
	case TINYRL_MATCH:
		// everything is OK with the world... 
		result = tinyrl_insert_text(tinyrl, " ");
		break;
	case TINYRL_NO_MATCH:
	case TINYRL_MATCH_WITH_EXTENSIONS:
	case TINYRL_AMBIGUOUS:
	case TINYRL_COMPLETED_AMBIGUOUS:
		// oops don't change the result and let the bell ring 
		break;
	}
*/
	// Happy compiler
	tinyrl = tinyrl;
	key = key;

	return result;
}
