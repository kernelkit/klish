#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <sys/wait.h>
#include <errno.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#include <faux/faux.h>
#include <faux/str.h>
#include <faux/msg.h>
#include <faux/list.h>
#include <faux/file.h>
#include <faux/eloop.h>
#include <klish/ktp.h>
#include <klish/ktp_session.h>
#include <tinyrl/tinyrl.h>

#include "private.h"


// Client mode
typedef enum {
	MODE_CMDLINE,
	MODE_FILES,
	MODE_STDIN,
	MODE_INTERACTIVE
} client_mode_e;


// Context for main loop
typedef struct ctx_s {
	ktp_session_t *ktp;
	tinyrl_t *tinyrl;
	struct options *opts;
	char *hotkeys[VT100_HOTKEY_MAP_LEN]; // MODE_INTERACTIVE
	// pager_working flag values:
	// TRI_UNDEFINED - Not started yet or not necessary
	// TRI_TRUE - Pager is working
	// TRI_FALSE - Can't start pager or pager has exited
	tri_t pager_working;
	FILE *pager_pipe;
	client_mode_e mode;
	// Parsing state vars
	faux_list_node_t *cmdline_iter; // MODE_CMDLINE
	faux_list_node_t *files_iter; // MODE_FILES
	faux_file_t *files_fd; // MODE_FILES
	faux_file_t *stdin_fd; // MODE_STDIN
} ctx_t;


// KTP session static functions
static bool_t async_stdin_sent_cb(ktp_session_t *ktp, size_t len,
	void *user_data);
static bool_t stdout_cb(ktp_session_t *ktp, const char *line, size_t len,
	void *user_data);
static bool_t stderr_cb(ktp_session_t *ktp, const char *line, size_t len,
	void *user_data);
static bool_t auth_ack_cb(ktp_session_t *ktp, const faux_msg_t *msg, void *udata);
static bool_t cmd_ack_cb(ktp_session_t *ktp, const faux_msg_t *msg, void *udata);
static bool_t cmd_incompleted_ack_cb(ktp_session_t *ktp, const faux_msg_t *msg, void *udata);
static bool_t completion_ack_cb(ktp_session_t *ktp, const faux_msg_t *msg, void *udata);
static bool_t help_ack_cb(ktp_session_t *ktp, const faux_msg_t *msg, void *udata);

// Eloop callbacks
//static bool_t stop_loop_ev(faux_eloop_t *eloop, faux_eloop_type_e type,
//	void *associated_data, void *user_data);
static bool_t stdin_cb(faux_eloop_t *eloop, faux_eloop_type_e type,
	void *associated_data, void *user_data);
static bool_t sigwinch_cb(faux_eloop_t *eloop, faux_eloop_type_e type,
	void *associated_data, void *user_data);
static bool_t ctrl_c_cb(faux_eloop_t *eloop, faux_eloop_type_e type,
	void *associated_data, void *user_data);

// Service functions
static void reset_hotkey_table(ctx_t *ctx);
static bool_t send_winch_notification(ctx_t *ctx);
static bool_t send_next_command(ctx_t *ctx);
static void signal_handler_empty(int signo);

// Keys
static bool_t tinyrl_key_enter(tinyrl_t *tinyrl, unsigned char key);
static bool_t tinyrl_key_tab(tinyrl_t *tinyrl, unsigned char key);
static bool_t tinyrl_key_help(tinyrl_t *tinyrl, unsigned char key);
static bool_t tinyrl_key_hotkey(tinyrl_t *tinyrl, unsigned char key);


int main(int argc, char **argv)
{
	int retval = -1;
	struct options *opts = NULL;
	int unix_sock = -1;
	ktp_session_t *ktp = NULL;
	int retcode = 0;
	faux_eloop_t *eloop = NULL;
	ctx_t ctx = {};
	tinyrl_t *tinyrl = NULL;
	char *hist_path = NULL;
	int stdin_flags = 0;
	struct sigaction sig_act = {};
	sigset_t sig_set = {};

#ifdef HAVE_LOCALE_H
	// Set current locale
	setlocale(LC_ALL, "");
#endif

	// Parse command line options
	opts = opts_init();
	if (opts_parse(argc, argv, opts)) {
		fprintf(stderr, "Error: Can't parse command line options\n");
		goto err;
	}

	// Parse config file
	if (!access(opts->cfgfile, R_OK)) {
		if (!config_parse(opts->cfgfile, opts))
			goto err;
	} else if (opts->cfgfile_userdefined) {
		// User defined config must be found
		fprintf(stderr, "Error: Can't find config file %s\n",
			opts->cfgfile);
		goto err;
	}

	// Init context
	faux_bzero(&ctx, sizeof(ctx));

	// Find out client mode
	ctx.mode = MODE_INTERACTIVE; // Default
	if (faux_list_len(opts->commands) > 0)
		ctx.mode = MODE_CMDLINE;
	else if (faux_list_len(opts->files) > 0)
		ctx.mode = MODE_FILES;
	else if (!isatty(STDIN_FILENO))
		ctx.mode = MODE_STDIN;

	// Connect to server
	unix_sock = ktp_connect_unix(opts->unix_socket_path);
	if (unix_sock < 0) {
		fprintf(stderr, "Error: Can't connect to server\n");
		goto err;
	}

	// Eloop object
	eloop = faux_eloop_new(NULL);
	// Handlers are used to send SIGINT
	// to non-interactive commands
	faux_eloop_add_signal(eloop, SIGINT, ctrl_c_cb, &ctx);
	faux_eloop_add_signal(eloop, SIGTERM, ctrl_c_cb, &ctx);
	faux_eloop_add_signal(eloop, SIGQUIT, ctrl_c_cb, &ctx);
	// To don't stop klish client on exit. SIGTSTP can be pended
	faux_eloop_add_signal(eloop, SIGTSTP, ctrl_c_cb, &ctx);
	// Notify server about terminal window size change
	faux_eloop_add_signal(eloop, SIGWINCH, sigwinch_cb, &ctx);

	// Ignore SIGINT etc. Don't use SIG_IGN because it will
	// not interrupt syscall. It necessary because in MODE_STDIN
	// there is a blocking read and eloop doesn't process signals
	// while blocking read
	sigemptyset(&sig_set);
	sig_act.sa_flags = 0;
	sig_act.sa_mask = sig_set;
	sig_act.sa_handler = &signal_handler_empty;
	sigaction(SIGINT, &sig_act, NULL);
	sigaction(SIGTERM, &sig_act, NULL);
	sigaction(SIGQUIT, &sig_act, NULL);
	// Ignore SIGPIPE from server. Don't use SIG_IGN because it will not
	// break syscall
	sigaction(SIGPIPE, &sig_act, NULL);

	// KTP session
	ktp = ktp_session_new(unix_sock, eloop);
	assert(ktp);
	if (!ktp) {
		fprintf(stderr, "Error: Can't create klish session\n");
		goto err;
	}
	// Don't stop loop on each answer
	ktp_session_set_stop_on_answer(ktp, BOOL_FALSE);

	// Set stdin to O_NONBLOCK mode
	stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	if (ctx.mode != MODE_STDIN)
		fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);

	// TiniRL
	if (ctx.mode == MODE_INTERACTIVE)
		hist_path = faux_expand_tilde("~/.klish_history");
	tinyrl = tinyrl_new(stdin, stdout, hist_path, opts->hist_size);
	if (ctx.mode == MODE_INTERACTIVE)
		faux_str_free(hist_path);
	tinyrl_set_prompt(tinyrl, "$ ");
	tinyrl_set_udata(tinyrl, &ctx);

	// Populate context
	ctx.ktp = ktp;
	ctx.tinyrl = tinyrl;
	ctx.opts = opts;
	ctx.pager_working = TRI_UNDEFINED;

	ktp_session_set_cb(ktp, KTP_SESSION_CB_STDIN, async_stdin_sent_cb, &ctx);
	ktp_session_set_cb(ktp, KTP_SESSION_CB_STDOUT, stdout_cb, &ctx);
	ktp_session_set_cb(ktp, KTP_SESSION_CB_STDERR, stderr_cb, &ctx);
	ktp_session_set_cb(ktp, KTP_SESSION_CB_AUTH_ACK, auth_ack_cb, &ctx);
	ktp_session_set_cb(ktp, KTP_SESSION_CB_CMD_ACK, cmd_ack_cb, &ctx);
	ktp_session_set_cb(ktp, KTP_SESSION_CB_CMD_ACK_INCOMPLETED,
		cmd_incompleted_ack_cb, &ctx);
	ktp_session_set_cb(ktp, KTP_SESSION_CB_COMPLETION_ACK,
		completion_ack_cb, &ctx);
	ktp_session_set_cb(ktp, KTP_SESSION_CB_HELP_ACK, help_ack_cb, &ctx);

	// Commands from cmdline
	if (ctx.mode == MODE_CMDLINE) {
		// "-c" options iterator
		ctx.cmdline_iter = faux_list_head(opts->commands);

	// Commands from files
	} else if (ctx.mode == MODE_FILES) {
		// input files iterator
		ctx.files_iter = faux_list_head(opts->files);

	// Commands from non-interactive STDIN
	} else if (ctx.mode == MODE_STDIN) {

	// Interactive shell
	} else {
		// Interactive keys
		tinyrl_set_hotkey_fn(tinyrl, tinyrl_key_hotkey);
		tinyrl_bind_key(tinyrl, '\n', tinyrl_key_enter);
		tinyrl_bind_key(tinyrl, '\r', tinyrl_key_enter);
		tinyrl_bind_key(tinyrl, '\t', tinyrl_key_tab);
		tinyrl_bind_key(tinyrl, '?', tinyrl_key_help);
		faux_eloop_add_fd(eloop, STDIN_FILENO, POLLIN, stdin_cb, &ctx);
	}

	// Send AUTH message to server
	if (!ktp_session_auth(ktp, NULL))
		goto err;

	// Main loop
	faux_eloop_loop(eloop);

	retval = 0;
err:
	// Restore stdin mode
	fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
	reset_hotkey_table(&ctx);
	if (tinyrl) {
		if (tinyrl_busy(tinyrl))
			faux_error_free(ktp_session_error(ktp));
		tinyrl_free(tinyrl);
	}
	ktp_session_free(ktp);
	faux_eloop_free(eloop);
	ktp_disconnect(unix_sock);
	opts_free(opts);

	if ((retval < 0) || (retcode != 0))
		return -1;

	return 0;
}


static bool_t send_next_command(ctx_t *ctx)
{
	char *line = NULL;
	faux_error_t *error = NULL;
	bool_t rc = BOOL_FALSE;

	// User must type next interactive command. So just return
	if (ctx->mode == MODE_INTERACTIVE)
		return BOOL_TRUE;

	// Commands from cmdline
	if (ctx->mode == MODE_CMDLINE) {
		line = faux_str_dup(faux_list_each(&ctx->cmdline_iter));

	// Commands from input files
	} else if (ctx->mode == MODE_FILES) {
		do {
			if (!ctx->files_fd) {
				const char *fn = (const char *)faux_list_each(&ctx->files_iter);
				if (!fn)
					break; // No more files
				ctx->files_fd = faux_file_open(fn, O_RDONLY, 0);
			}
			if (!ctx->files_fd) // Can't open file. Try next file
				continue;
			line = faux_file_getline(ctx->files_fd);
			if (!line) { // EOF
				faux_file_close(ctx->files_fd);
				ctx->files_fd = NULL;
			}
		} while (!line);

	// Commands from stdin
	} else if (ctx->mode == MODE_STDIN) {
		if (!ctx->stdin_fd)
			ctx->stdin_fd = faux_file_fdopen(STDIN_FILENO);
		if (ctx->stdin_fd)
			line = faux_file_getline(ctx->stdin_fd);
		if (!line) // EOF
			faux_file_close(ctx->stdin_fd);
	}

	if (!line) {
		ktp_session_set_done(ctx->ktp, BOOL_TRUE);
		return BOOL_TRUE;
	}

	if (ctx->opts->verbose) {
		const char *prompt = tinyrl_prompt(ctx->tinyrl);
		printf("%s%s\n", prompt ? prompt : "", line);
		fflush(stdout);
	}

	error = faux_error_new();
	rc = ktp_session_cmd(ctx->ktp, line, error, ctx->opts->dry_run);
	faux_str_free(line);
	if (!rc) {
		faux_error_free(error);
		return BOOL_FALSE;
	}

	// Suppose non-interactive command by default
	tinyrl_enable_isig(ctx->tinyrl);

	return BOOL_TRUE;
}


static bool_t stderr_cb(ktp_session_t *ktp, const char *line, size_t len,
	void *user_data)
{
	if (faux_write_block(STDERR_FILENO, line, len) < 0)
		return BOOL_FALSE;

	ktp = ktp;
	user_data = user_data;

	return BOOL_TRUE;
}


static bool_t process_prompt_param(tinyrl_t *tinyrl, const faux_msg_t *msg)
{
	char *prompt = NULL;

	if (!tinyrl)
		return BOOL_FALSE;
	if (!msg)
		return BOOL_FALSE;

	prompt = faux_msg_get_str_param_by_type(msg, KTP_PARAM_PROMPT);
	if (prompt) {
		tinyrl_set_prompt(tinyrl, prompt);
		faux_str_free(prompt);
	}

	return BOOL_TRUE;
}


static void reset_hotkey_table(ctx_t *ctx)
{
	size_t i = 0;

	assert(ctx);

	for (i = 0; i < VT100_HOTKEY_MAP_LEN; i++)
		faux_str_free(ctx->hotkeys[i]);
	faux_bzero(ctx->hotkeys, sizeof(ctx->hotkeys));
}


static bool_t process_hotkey_param(ctx_t *ctx, const faux_msg_t *msg)
{
	faux_list_node_t *iter = NULL;
	uint32_t param_len = 0;
	char *param_data = NULL;
	uint16_t param_type = 0;

	if (!ctx)
		return BOOL_FALSE;
	if (!msg)
		return BOOL_FALSE;

	if (!faux_msg_get_param_by_type(msg, KTP_PARAM_HOTKEY,
		(void **)&param_data, &param_len))
		return BOOL_TRUE;

	// If there is HOTKEY parameter then reinitialize whole hotkey table
	reset_hotkey_table(ctx);

	iter = faux_msg_init_param_iter(msg);
	while (faux_msg_get_param_each(
		&iter, &param_type, (void **)&param_data, &param_len)) {
		char *cmd = NULL;
		ssize_t code = -1;
		size_t key_len = 0;

		if (param_len < 3) // <key>'\0'<cmd>
			continue;
		if (KTP_PARAM_HOTKEY != param_type)
			continue;
		key_len = strlen(param_data); // Length of <key>
		if (key_len < 1)
			continue;
		code = vt100_hotkey_decode(param_data);
		if ((code < 0) || (code > VT100_HOTKEY_MAP_LEN))
			continue;
		cmd = faux_str_dupn(param_data + key_len + 1,
			param_len - key_len - 1);
		if (!cmd)
			continue;
		ctx->hotkeys[code] = cmd;
		tinyrl_unbind_key(ctx->tinyrl, code);
	}

	return BOOL_TRUE;
}


bool_t auth_ack_cb(ktp_session_t *ktp, const faux_msg_t *msg, void *udata)
{
	ctx_t *ctx = (ctx_t *)udata;
	int rc = -1;
	faux_error_t *error = NULL;

	process_prompt_param(ctx->tinyrl, msg);
	process_hotkey_param(ctx, msg);

	if (!ktp_session_retcode(ktp, &rc))
		rc = -1;
	error = ktp_session_error(ktp);
	if ((rc < 0) && (faux_error_len(error) > 0)) {
		faux_error_node_t *err_iter = faux_error_iter(error);
		const char *err = NULL;
		while ((err = faux_error_each(&err_iter)))
			fprintf(stderr, "Error: auth: %s\n", err);
		return BOOL_FALSE;
	}

	send_winch_notification(ctx);

	if (ctx->mode == MODE_INTERACTIVE) {
		// Print prompt for interactive command
		tinyrl_redisplay(ctx->tinyrl);
	} else {
		// Send first command for non-interactive modes
		send_next_command(ctx);
	}

	// Happy compiler
	msg = msg;

	return BOOL_TRUE;
}


bool_t cmd_ack_cb(ktp_session_t *ktp, const faux_msg_t *msg, void *udata)
{
	ctx_t *ctx = (ctx_t *)udata;
	int rc = -1;
	faux_error_t *error = NULL;
	bool_t it_was_pager = BOOL_FALSE;

	// Wait for pager
	if (ctx->pager_working != TRI_UNDEFINED) {
		pclose(ctx->pager_pipe);
		ctx->pager_working = TRI_UNDEFINED;
		ctx->pager_pipe = NULL;
		it_was_pager = BOOL_TRUE;
	}

	// Disable SIGINT caught for non-interactive commands.
	// Do it after pager exit. Else it can restore wrong tty mode after
	// ISIG disabling
	tinyrl_disable_isig(ctx->tinyrl);

	// Sometimes output stream from server doesn't contain final crlf so
	// goto newline itself
	if (ktp_session_last_stream(ktp) == STDERR_FILENO) {
		if (ktp_session_stderr_need_newline(ktp))
			fprintf(stderr, "\n");
	} else {
		// Pager adds newline itself
		if (ktp_session_stdout_need_newline(ktp) && !it_was_pager)
			tinyrl_crlf(ctx->tinyrl);
	}

	process_prompt_param(ctx->tinyrl, msg);
	process_hotkey_param(ctx, msg);

	if (!ktp_session_retcode(ktp, &rc))
		rc = -1;
	error = ktp_session_error(ktp);
	if (rc < 0) {
		if (faux_error_len(error) > 0) {
			faux_error_node_t *err_iter = faux_error_iter(error);
			const char *err = NULL;
			while ((err = faux_error_each(&err_iter)))
				fprintf(stderr, "Error: %s\n", err);
		}
		// Stop-on-error
		if (ctx->opts->stop_on_error) {
			faux_error_free(error);
			ktp_session_set_done(ktp, BOOL_TRUE);
			return BOOL_TRUE;
		}
	}
	faux_error_free(error);

	if (ctx->mode == MODE_INTERACTIVE) {
		tinyrl_set_busy(ctx->tinyrl, BOOL_FALSE);
		if (!ktp_session_done(ktp))
			tinyrl_redisplay(ctx->tinyrl);
		// Operation is finished so restore stdin handler
		faux_eloop_add_fd(ktp_session_eloop(ktp), STDIN_FILENO, POLLIN,
			stdin_cb, ctx);
	}

	// Send next command for non-interactive modes
	send_next_command(ctx);

	// Happy compiler
	msg = msg;

	return BOOL_TRUE;
}


bool_t cmd_incompleted_ack_cb(ktp_session_t *ktp, const faux_msg_t *msg, void *udata)
{
	ctx_t *ctx = (ctx_t *)udata;

	// It can't get data from stdin, it gets commands from stdin instead.
	// So don't process incompleted ack but just return
	if (ctx->mode == MODE_STDIN)
		return BOOL_TRUE;

	if (ktp_session_state(ktp) == KTP_SESSION_STATE_WAIT_FOR_CMD) {
		// Cmd need stdin so restore stdin handler
		if (KTP_STATUS_IS_NEED_STDIN(ktp_session_cmd_features(ktp))) {
			// Disable SIGINT signal (it is used for commands that
			// don't need stdin. Commands with stdin can get ^C
			// themself interactively.)
			tinyrl_disable_isig(ctx->tinyrl);
			faux_eloop_add_fd(ktp_session_eloop(ktp), STDIN_FILENO, POLLIN,
				stdin_cb, ctx);
		}
	}

	// Happy compiler
	msg = msg;

	return BOOL_TRUE;
}


static bool_t stdin_cb(faux_eloop_t *eloop, faux_eloop_type_e type,
	void *associated_data, void *udata)
{
	bool_t rc = BOOL_TRUE;
	ctx_t *ctx = (ctx_t *)udata;
	ktp_session_state_e state = KTP_SESSION_STATE_ERROR;
	faux_eloop_info_fd_t *info = (faux_eloop_info_fd_t *)associated_data;
	bool_t close_stdin = BOOL_FALSE;
	size_t obuf_len = 0;

	if (!ctx)
		return BOOL_FALSE;

	// Some errors or fd is closed so stop interactive session
	// Non-interactive session just removes stdin callback
	if (info->revents & (POLLHUP | POLLERR | POLLNVAL))
		close_stdin = BOOL_TRUE;

	// Temporarily stop stdin reading because too much data is buffered
	// and all data can't be sent to server yet
	obuf_len = faux_buf_len(faux_async_obuf(ktp_session_async(ctx->ktp)));
	if (obuf_len > OBUF_LIMIT) {
		faux_eloop_del_fd(eloop, STDIN_FILENO);
		return BOOL_TRUE;
	}

	state = ktp_session_state(ctx->ktp);

	// Standard klish command line
	if ((state == KTP_SESSION_STATE_IDLE) &&
		(ctx->mode == MODE_INTERACTIVE)) {
		tinyrl_read(ctx->tinyrl);
		if (close_stdin) {
			faux_eloop_del_fd(eloop, STDIN_FILENO);
			rc = BOOL_FALSE;
		}

	// Command needs stdin
	} else if ((state == KTP_SESSION_STATE_WAIT_FOR_CMD) &&
		KTP_STATUS_IS_NEED_STDIN(ktp_session_cmd_features(ctx->ktp))) {
		int fd = fileno(tinyrl_istream(ctx->tinyrl));
		char buf[1024] = {};
		ssize_t bytes_readed = 0;

		// Don't read all data from stdin to don't overfill out buffer.
		// Allow another handlers to push already received data to
		// server
		if ((bytes_readed = read(fd, buf, sizeof(buf))) > 0)
			ktp_session_stdin(ctx->ktp, buf, bytes_readed);
		// Actually close stdin only when all data is read
		if (close_stdin && (bytes_readed <= 0)) {
			ktp_session_stdin_close(ctx->ktp);
			faux_eloop_del_fd(eloop, STDIN_FILENO);
		}

	// Input is not needed
	} else {
		// Here the situation when input is not allowed. Remove stdin from
		// eloop waiting list. Else klish will get 100% CPU. Callbacks on
		// operation completions will restore this handler.
		faux_eloop_del_fd(eloop, STDIN_FILENO);
	}

	// Happy compiler
	type = type;

	return rc;
}


static bool_t async_stdin_sent_cb(ktp_session_t *ktp, size_t len,
	void *user_data)
{
	ctx_t *ctx = (ctx_t *)user_data;

	assert(ktp);

	// This callbacks is executed when any number of bytes is really written
	// to server socket. So if stdin transmit was stopped due to obuf
	// overflow it's time to rearm transmission
	faux_eloop_add_fd(ktp_session_eloop(ktp), STDIN_FILENO, POLLIN,
		stdin_cb, ctx);

	len = len; // Happy compiler

	return BOOL_TRUE;
}


static bool_t send_winch_notification(ctx_t *ctx)
{
	size_t width = 0;
	size_t height = 0;
	char *winsize = NULL;
	faux_msg_t *req = NULL;
	ktp_status_e status = KTP_STATUS_NONE;

	if (!ctx->tinyrl)
		return BOOL_FALSE;
	if (!ctx->ktp)
		return BOOL_FALSE;

	tinyrl_winsize(ctx->tinyrl, &width, &height);
	winsize = faux_str_sprintf("%lu %lu", width, height);

	req = ktp_msg_preform(KTP_NOTIFICATION, status);
	faux_msg_add_param(req, KTP_PARAM_WINCH, winsize, strlen(winsize));
	faux_str_free(winsize);
	faux_msg_send_async(req, ktp_session_async(ctx->ktp));
	faux_msg_free(req);

	return BOOL_TRUE;
}


static bool_t sigwinch_cb(faux_eloop_t *eloop, faux_eloop_type_e type,
	void *associated_data, void *udata)
{
	ctx_t *ctx = (ctx_t *)udata;

	if (!ctx)
		return BOOL_FALSE;

	send_winch_notification(ctx);

	// Happy compiler
	eloop = eloop;
	type = type;
	associated_data = associated_data;

	return BOOL_TRUE;
}


static bool_t ctrl_c_cb(faux_eloop_t *eloop, faux_eloop_type_e type,
	void *associated_data, void *udata)
{
	ctx_t *ctx = (ctx_t *)udata;
	char ctrl_c = KEY_ETX;
	ktp_session_state_e state = KTP_SESSION_STATE_ERROR;

	if (!ctx)
		return BOOL_FALSE;

	if (ctx->mode != MODE_INTERACTIVE)
		return BOOL_FALSE;

	state = ktp_session_state(ctx->ktp);
	if (state == KTP_SESSION_STATE_WAIT_FOR_CMD)
		ktp_session_stdin(ctx->ktp, &ctrl_c, sizeof(ctrl_c));

	// Happy compiler
	eloop = eloop;
	type = type;
	associated_data = associated_data;

	return BOOL_TRUE;
}


static bool_t tinyrl_passive_line(const char *line)
{
	const char *pos = line;

	if (faux_str_is_empty(line))
		return BOOL_TRUE;

	while (*pos && isspace(*pos))
		pos++;
	if (*pos == '#')
		return BOOL_TRUE;

	return BOOL_FALSE;
}


static bool_t tinyrl_key_enter(tinyrl_t *tinyrl, unsigned char key)
{
	const char *line = NULL;
	ctx_t *ctx = (ctx_t *)tinyrl_udata(tinyrl);
	faux_error_t *error = faux_error_new();

	tinyrl_line_to_hist(tinyrl);
	tinyrl_multi_crlf(tinyrl);
	tinyrl_reset_line_state(tinyrl);
	line = tinyrl_line(tinyrl);

	// Don't do anything on empty or commented-out lines
	if (tinyrl_passive_line(line)) {
		tinyrl_reset_line(tinyrl);
		faux_error_free(error);
		return BOOL_TRUE;
	}

	ktp_session_cmd(ctx->ktp, line, error, ctx->opts->dry_run);

	tinyrl_reset_line(tinyrl);
	tinyrl_set_busy(tinyrl, BOOL_TRUE);
	// Suppose non-interactive command by default
	// Caught SIGINT for non-interactive commands
	tinyrl_enable_isig(tinyrl);

	key = key; // Happy compiler

	return BOOL_TRUE;
}


static bool_t tinyrl_key_hotkey(tinyrl_t *tinyrl, unsigned char key)
{
	const char *line = NULL;
	ctx_t *ctx = (ctx_t *)tinyrl_udata(tinyrl);
	faux_error_t *error = NULL;

	if (key >= VT100_HOTKEY_MAP_LEN)
		return BOOL_TRUE;
	line = ctx->hotkeys[key];
	if (faux_str_is_empty(line))
		return BOOL_TRUE;

	error = faux_error_new();
	tinyrl_multi_crlf(tinyrl);
	tinyrl_reset_line_state(tinyrl);
	tinyrl_reset_line(tinyrl);

	ktp_session_cmd(ctx->ktp, line, error, ctx->opts->dry_run);

	tinyrl_set_busy(tinyrl, BOOL_TRUE);

	return BOOL_TRUE;
}


static bool_t tinyrl_key_tab(tinyrl_t *tinyrl, unsigned char key)
{
	char *line = NULL;
	ctx_t *ctx = (ctx_t *)tinyrl_udata(tinyrl);

	line = tinyrl_line_to_pos(tinyrl);
	ktp_session_completion(ctx->ktp, line, ctx->opts->dry_run);
	faux_str_free(line);

	tinyrl_set_busy(tinyrl, BOOL_TRUE);

	key = key; // Happy compiler

	return BOOL_TRUE;
}


static bool_t tinyrl_key_help(tinyrl_t *tinyrl, unsigned char key)
{
	char *line = NULL;
	ctx_t *ctx = (ctx_t *)tinyrl_udata(tinyrl);

	line = tinyrl_line_to_pos(tinyrl);
	// If "?" is quoted then it's not special hotkey.
	// Just insert it into the line.
	if (faux_str_unclosed_quotes(line, NULL)) {
		faux_str_free(line);
		return tinyrl_key_default(tinyrl, key);
	}

	ktp_session_help(ctx->ktp, line);
	faux_str_free(line);

	tinyrl_set_busy(tinyrl, BOOL_TRUE);

	key = key; // Happy compiler

	return BOOL_TRUE;
}


static void display_completions(const tinyrl_t *tinyrl, faux_list_t *completions,
	const char *prefix, size_t max)
{
	size_t width = tinyrl_width(tinyrl);
	size_t cols = 0;
	faux_list_node_t *iter = NULL;
	faux_list_node_t *node = NULL;
	size_t prefix_len = 0;
	size_t cols_filled = 0;

	if (prefix)
		prefix_len = strlen(prefix);

	// Find out column and rows number
	if (max < width)
		cols = (width + 1) / (prefix_len + max + 1); // For a space between words
	else
		cols = 1;

	iter = faux_list_head(completions);
	while ((node = faux_list_each_node(&iter))) {
		char *compl = (char *)faux_list_data(node);
		tinyrl_printf(tinyrl, "%*s%s",
			(prefix_len + max + 1 - strlen(compl)),
			prefix ? prefix : "",
			compl);
		cols_filled++;
		if ((cols_filled >= cols) || (node == faux_list_tail(completions))) {
			cols_filled = 0;
			tinyrl_crlf(tinyrl);
		}
	}
}


bool_t completion_ack_cb(ktp_session_t *ktp, const faux_msg_t *msg, void *udata)
{
	ctx_t *ctx = (ctx_t *)udata;
	faux_list_node_t *iter = NULL;
	uint32_t param_len = 0;
	char *param_data = NULL;
	uint16_t param_type = 0;
	char *prefix = NULL;
	faux_list_t *completions = NULL;
	size_t completions_num = 0;
	size_t max_compl_len = 0;

	tinyrl_set_busy(ctx->tinyrl, BOOL_FALSE);

	process_prompt_param(ctx->tinyrl, msg);

	prefix = faux_msg_get_str_param_by_type(msg, KTP_PARAM_PREFIX);

	completions = faux_list_new(FAUX_LIST_UNSORTED, FAUX_LIST_NONUNIQUE,
		NULL, NULL, (void (*)(void *))faux_str_free);

	iter = faux_msg_init_param_iter(msg);
	while (faux_msg_get_param_each(&iter, &param_type, (void **)&param_data, &param_len)) {
		char *compl = NULL;
		if (KTP_PARAM_LINE != param_type)
			continue;
		compl = faux_str_dupn(param_data, param_len);
		faux_list_add(completions, compl);
		if (param_len > max_compl_len)
			max_compl_len = param_len;
	}

	completions_num = faux_list_len(completions);

	// Single possible completion
	if (1 == completions_num) {
		char *compl = (char *)faux_list_data(faux_list_head(completions));
		tinyrl_line_insert(ctx->tinyrl, compl, strlen(compl));
		// Add space after completion
		tinyrl_line_insert(ctx->tinyrl, " ", 1);
		tinyrl_redisplay(ctx->tinyrl);

	// Multi possible completions
	} else if (completions_num > 1) {
		faux_list_node_t *eq_iter = NULL;
		size_t eq_part = 0;
		char *str = NULL;
		char *compl = NULL;

		// Try to find equal part for all possible completions
		eq_iter = faux_list_head(completions);
		str = (char *)faux_list_data(eq_iter);
		eq_part = strlen(str);
		eq_iter = faux_list_next_node(eq_iter);

		while ((compl = (char *)faux_list_each(&eq_iter)) && (eq_part > 0)) {
			size_t cur_eq = 0;
			cur_eq = tinyrl_equal_part(ctx->tinyrl, str, compl);
			if (cur_eq < eq_part)
				eq_part = cur_eq;
		}

		// The equal part was found
		if (eq_part > 0) {
			tinyrl_line_insert(ctx->tinyrl, str, eq_part);
			tinyrl_redisplay(ctx->tinyrl);

		// There is no equal part for all completions
		} else {
			tinyrl_multi_crlf(ctx->tinyrl);
			tinyrl_reset_line_state(ctx->tinyrl);
			display_completions(ctx->tinyrl, completions,
				prefix, max_compl_len);
			tinyrl_redisplay(ctx->tinyrl);
		}
	}

	faux_list_free(completions);
	faux_str_free(prefix);

	// Operation is finished so restore stdin handler
	faux_eloop_add_fd(ktp_session_eloop(ktp), STDIN_FILENO, POLLIN,
		stdin_cb, ctx);

	// Happy compiler
	ktp = ktp;
	msg = msg;

	return BOOL_TRUE;
}


static void display_help(const tinyrl_t *tinyrl, faux_list_t *help_list,
	size_t max)
{
	faux_list_node_t *iter = NULL;
	faux_list_node_t *node = NULL;

	iter = faux_list_head(help_list);
	while ((node = faux_list_each_node(&iter))) {
		help_t *help = (help_t *)faux_list_data(node);
		tinyrl_printf(tinyrl, "  %s%*s%s\n",
			help->prefix,
			(max + 2 - strlen(help->prefix)),
			" ",
			help->line);
	}
}


bool_t help_ack_cb(ktp_session_t *ktp, const faux_msg_t *msg, void *udata)
{
	ctx_t *ctx = (ctx_t *)udata;
	faux_list_t *help_list = NULL;
	faux_list_node_t *iter = NULL;
	uint32_t param_len = 0;
	char *param_data = NULL;
	uint16_t param_type = 0;
	size_t max_prefix_len = 0;

	tinyrl_set_busy(ctx->tinyrl, BOOL_FALSE);

	process_prompt_param(ctx->tinyrl, msg);

	help_list = faux_list_new(FAUX_LIST_SORTED, FAUX_LIST_UNIQUE,
		help_compare, NULL, help_free);

	// Wait for PREFIX - LINE pairs
	iter = faux_msg_init_param_iter(msg);
	while (faux_msg_get_param_each(&iter, &param_type, (void **)&param_data,
		&param_len)) {
		char *prefix_str = NULL;
		char *line_str = NULL;
		help_t *help = NULL;
		size_t prefix_len = 0;

		// Get PREFIX
		if (KTP_PARAM_PREFIX != param_type)
			continue;
		prefix_str = faux_str_dupn(param_data, param_len);
		prefix_len = param_len;

		// Get LINE
		if (!faux_msg_get_param_each(&iter, &param_type,
			(void **)&param_data, &param_len) ||
			(KTP_PARAM_LINE != param_type)) {
			faux_str_free(prefix_str);
			break;
		}
		line_str = faux_str_dupn(param_data, param_len);

		help = help_new(prefix_str, line_str);
		if (!faux_list_add(help_list, help)) {
			help_free(help);
			continue;
		}
		if (prefix_len > max_prefix_len)
			max_prefix_len = prefix_len;
	}

	if (faux_list_len(help_list) > 0) {
		tinyrl_multi_crlf(ctx->tinyrl);
		tinyrl_reset_line_state(ctx->tinyrl);
		display_help(ctx->tinyrl, help_list, max_prefix_len);
		tinyrl_redisplay(ctx->tinyrl);
	}

	faux_list_free(help_list);

	// Operation is finished so restore stdin handler
	faux_eloop_add_fd(ktp_session_eloop(ktp), STDIN_FILENO, POLLIN,
		stdin_cb, ctx);

	ktp = ktp; // happy compiler

	return BOOL_TRUE;
}


//size_t max_stdout_len = 0;

static bool_t stdout_cb(ktp_session_t *ktp, const char *line, size_t len,
	void *udata)
{
	ctx_t *ctx = (ctx_t *)udata;

	assert(ctx);

//if (len > max_stdout_len) {
//max_stdout_len = len;
//fprintf(stderr, "max_stdout_len=%ld\n", max_stdout_len);
//}

	// Start pager if necessary
	if (
		ctx->opts->pager_enabled && // Pager enabled within config file
		(ctx->pager_working == TRI_UNDEFINED) && // Pager is not working
		!KTP_STATUS_IS_INTERACTIVE(ktp_session_cmd_features(ktp)) // Non interactive command
		) {

		ctx->pager_pipe = popen(ctx->opts->pager, "we");
		if (!ctx->pager_pipe)
			ctx->pager_working = TRI_FALSE; // Indicates can't start
		else
			ctx->pager_working = TRI_TRUE;
	}

	// Write to pager's pipe if pager is really working
	// Don't do anything if pager state is TRI_FALSE
	if (ctx->pager_working == TRI_TRUE) {
		if (faux_write_block(fileno(ctx->pager_pipe), line, len) <= 0) {
			// If we can't write to pager pipe then send
			// "SIGPIPE" to server. Pager is finished or broken.
			ktp_session_stdout_close(ktp);
			ctx->pager_working = TRI_FALSE;
			return BOOL_TRUE; // Don't break the loop
		}

	// TRI_UNDEFINED here means that pager is not needed
	} else if (ctx->pager_working == TRI_UNDEFINED) {
		if (faux_write_block(STDOUT_FILENO, line, len) < 0)
			return BOOL_FALSE;
	}

	return BOOL_TRUE;
}


static void signal_handler_empty(int signo)
{
	signo = signo; // Happy compiler
}
