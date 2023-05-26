#ifndef _tinyrl_tinyrl_h
#define _tinyrl_tinyrl_h

#include <stdio.h>

#include <faux/faux.h>

#include "tinyrl/vt100.h"

C_DECL_BEGIN

typedef struct tinyrl_s tinyrl_t;


typedef enum {
    /**
     * no possible completions were found
     */
	TINYRL_NO_MATCH = 0,
    /**
     * the provided string was already an exact match
     */
	TINYRL_MATCH,
    /**
     * the provided string was ambiguous and produced
     * more than one possible completion
     */
	TINYRL_AMBIGUOUS,
    /**
     * the provided string was unambiguous and a 
     * completion was performed
     */
	TINYRL_COMPLETED_MATCH,
    /**
     * the provided string was ambiguous but a partial
     * completion was performed.
     */
	TINYRL_COMPLETED_AMBIGUOUS,
    /**
     * the provided string was an exact match for one
     * possible value but there are other exetensions
     * of the string available.
     */
	TINYRL_MATCH_WITH_EXTENSIONS
} tinyrl_match_e;

/* virtual methods */
typedef char *tinyrl_compentry_func_t(tinyrl_t * instance,
	const char *text, unsigned offset, unsigned state);
typedef int tinyrl_hook_func_t(tinyrl_t * instance);

typedef char **tinyrl_completion_func_t(tinyrl_t * instance,
	const char *text, unsigned start, unsigned end);

typedef int tinyrl_timeout_fn_t(tinyrl_t *instance);

/**
 * \return
 * - BOOL_TRUE if the action associated with the key has
 *   been performed successfully
 * - BOOL_FALSE if the action was not successful
 */
typedef bool_t tinyrl_key_func_t(tinyrl_t *instance, unsigned char key);




tinyrl_t *tinyrl_new(FILE *istream, FILE *ostream,
	const char *hist_fname, size_t hist_stifle);
void tinyrl_free(tinyrl_t *tinyrl);

bool_t tinyrl_bind_key(tinyrl_t *tinyrl, int key, tinyrl_key_func_t *fn);
bool_t tinyrl_unbind_key(tinyrl_t *tinyrl, int key);
void tinyrl_set_hotkey_fn(tinyrl_t *tinyrl, tinyrl_key_func_t *fn);
void tinyrl_set_istream(tinyrl_t *tinyrl, FILE *istream);
FILE *tinyrl_istream(const tinyrl_t *tinyrl);
void tinyrl_set_ostream(tinyrl_t *tinyrl, FILE *ostream);
FILE *tinyrl_ostream(const tinyrl_t *tinyrl);
void tinyrl_set_utf8(tinyrl_t *tinyrl, bool_t utf8);
bool_t tinyrl_utf8(const tinyrl_t *tinyrl);
bool_t tinyrl_busy(const tinyrl_t *tinyrl);
void tinyrl_set_busy(tinyrl_t *tinyrl, bool_t busy);
void tinyrl_set_prompt(tinyrl_t *tinyrl, const char *prompt);
const char *tinyrl_prompt(const tinyrl_t *tinyrl);
const char *tinyrl_line(const tinyrl_t *tinyrl);
bool_t tinyrl_hist_save(const tinyrl_t *tinyrl);
bool_t tinyrl_hist_restore(tinyrl_t *tinyrl);
void tinyrl_line_to_hist(tinyrl_t *tinyrl);
void tinyrl_reset_hist_pos(tinyrl_t *tinyrl);
void *tinyrl_udata(const tinyrl_t *tinyrl);
void tinyrl_set_udata(tinyrl_t *tinyrl, void *udata);
void tty_raw_mode(tinyrl_t *tinyrl);
void tty_restore_mode(tinyrl_t *tinyrl);
int tinyrl_read(tinyrl_t *tinyrl);
void tinyrl_redisplay(tinyrl_t *tinyrl);
void tinyrl_save_last(tinyrl_t *tinyrl);
void tinyrl_reset_line_state(tinyrl_t *tinyrl);
void tinyrl_reset_line(tinyrl_t *tinyrl);
void tinyrl_crlf(const tinyrl_t *tinyrl);
void tinyrl_multi_crlf(const tinyrl_t *tinyrl);

void tinyrl_winsize(const tinyrl_t *tinyrl, size_t *width, size_t *height);
size_t tinyrl_width(const tinyrl_t *tinyrl);
size_t tinyrl_height(const tinyrl_t *tinyrl);

int tinyrl_printf(const tinyrl_t *tinyrl, const char *fmt, ...);
size_t tinyrl_equal_part(const tinyrl_t *tinyrl,
	const char *s1, const char *s2);


bool_t tinyrl_line_insert(tinyrl_t *tinyrl, const char *text, size_t len);
bool_t tinyrl_line_delete(tinyrl_t *tinyrl, size_t start, size_t len);
bool_t tinyrl_line_replace(tinyrl_t *tinyrl, const char *text);




extern void tinyrl_done(tinyrl_t * instance);

extern void tinyrl_completion_over(tinyrl_t * instance);

extern void tinyrl_completion_error_over(tinyrl_t * instance);

extern bool_t tinyrl_is_completion_error_over(const tinyrl_t * instance);


/**
 * This operation returns the current line in use by the tinyrl instance
 * NB. the pointer will become invalid after any further operation on the 
 * instance.
 */
extern void tinyrl_delete_matches(char **instance);
extern char **tinyrl_completion(tinyrl_t *instance,
	const char *line, unsigned start, unsigned end,
	tinyrl_compentry_func_t *generator);
extern void tinyrl_ding(const tinyrl_t * instance);



extern void
tinyrl_replace_line(tinyrl_t * instance, const char *text, int clear_undo);

/**
 * Complete the current word in the input buffer, displaying
 * a prompt to clarify any abiguity if necessary.
 *
 * \return
 * - the type of match performed.
 * \post
 * - If the current word is ambiguous then a list of 
 *   possible completions will be displayed.
 */
extern tinyrl_match_e tinyrl_complete(tinyrl_t * instance);

/**
 * Complete the current word in the input buffer, displaying
 * a prompt to clarify any abiguity or extra extensions if necessary.
 *
 * \return
 * - the type of match performed.
 * \post
 * - If the current word is ambiguous then a list of 
 *   possible completions will be displayed.
 * - If the current word is complete but there are extra
 *   completions which are an extension of that word then
 *   a list of these will be displayed.
 */
extern tinyrl_match_e tinyrl_complete_with_extensions(tinyrl_t * instance);

/**
 * Disable echoing of input characters when a line in input.
 * 
 */
extern void tinyrl_disable_echo(
	 /** 
          * The instance on which to operate
          */
				       tinyrl_t * instance,
	 /**
          * The character to display instead of a key press.
          *
          * If this has the special value '/0' then the insertion point will not 
          * be moved when keys are pressed.
          */
				       char echo_char);
/**
 * Enable key echoing for this instance. (This is the default behaviour)
 */
extern void tinyrl_enable_echo(
	/** 
         * The instance on which to operate
         */
				      tinyrl_t * instance);
/**
 * Indicate whether the current insertion point is quoting or not
 */
extern bool_t tinyrl_is_quoting(
	/** 
         * The instance on which to operate
         */
				       const tinyrl_t * instance);
/**
 * Indicate whether the current insertion is empty or not
 */
extern bool_t
	tinyrl_is_empty(
		/**
		 * The instance on which to operate
		 */
		const tinyrl_t *instance
	);
/**
 * Limit maximum line length
 */
extern void tinyrl_limit_line_length(
	/** 
         * The instance on which to operate
         */
					    tinyrl_t * instance,
	/** 
         * The length to limit to (0) is unlimited
         */
					    unsigned length);

C_DECL_END

#endif				/* _tinyrl_tinyrl_h */
