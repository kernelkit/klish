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

#include "private.h"


static bool_t stdout_cb(ktp_session_t *ktp, const char *line, size_t len,
	void *user_data);
static bool_t stderr_cb(ktp_session_t *ktp, const char *line, size_t len,
	void *user_data);
static bool_t stop_loop_ev(faux_eloop_t *eloop, faux_eloop_type_e type,
	void *associated_data, void *user_data);

static bool_t ktp_sync_cmd(ktp_session_t *ktp, const char *line,
	int *retcode, faux_error_t *error, bool_t dry_run);


int main(int argc, char **argv)
{
	int retval = -1;
	struct options *opts = NULL;
	int unix_sock = -1;
	ktp_session_t *ktp = NULL;
	int retcode = 0;
	faux_eloop_t *eloop = NULL;

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

	// Connect to server
	unix_sock = ktp_connect_unix(opts->unix_socket_path);
	if (unix_sock < 0) {
		fprintf(stderr, "Error: Can't connect to server\n");
		goto err;
	}

	// Eloop object
	eloop = faux_eloop_new(NULL);
	faux_eloop_add_signal(eloop, SIGINT, stop_loop_ev, NULL);
	faux_eloop_add_signal(eloop, SIGTERM, stop_loop_ev, NULL);
	faux_eloop_add_signal(eloop, SIGQUIT, stop_loop_ev, NULL);

	// KTP session
	ktp = ktp_session_new(unix_sock, eloop);
	assert(ktp);
	if (!ktp) {
		fprintf(stderr, "Error: Can't create klish session\n");
		goto err;
	}
	// These callback functions is only for batch mode. Interactive
	// mode can reassign them.
	ktp_session_set_cb(ktp, KTP_SESSION_CB_STDOUT, stdout_cb, NULL);
	ktp_session_set_cb(ktp, KTP_SESSION_CB_STDERR, stderr_cb, NULL);

	// Commands from cmdline
	if (faux_list_len(opts->commands) > 0) {
		const char *line = NULL;
		faux_list_node_t *iter = faux_list_head(opts->commands);
		while ((line = faux_list_each(&iter))) {
			faux_error_t *error = faux_error_new();
			bool_t rc = BOOL_FALSE;
			// Echo command
			if (!opts->quiet)
				fprintf(stderr, "%s\n", line);
			// Request to server
			rc = ktp_sync_cmd(ktp, line, &retcode,
				error, opts->dry_run);
			if (!rc)
				retcode = -1;
			if (faux_error_len(error) > 0) {
				fprintf(stderr, "Error:\n");
				faux_error_fshow(error, stderr);
			}
			faux_error_free(error);
//			fprintf(stderr, "Retcode: %d\n", retcode);
			// Stop-on-error
			if (opts->stop_on_error && (!rc || retcode != 0))
				break;
		}

	// Commands from files
	} else if (faux_list_len(opts->files) > 0) {
		const char *filename = NULL;
		faux_list_node_t *iter =  faux_list_head(opts->files);
		while ((filename = (const char *)faux_list_each(&iter))) {
			char *line = NULL;
			bool_t stop = BOOL_FALSE;
			faux_file_t *fd = faux_file_open(filename, O_RDONLY, 0);
			while ((line = faux_file_getline(fd))) {
				faux_error_t *error = faux_error_new();
				bool_t rc = BOOL_FALSE;
				// Echo command
				if (!opts->quiet)
					fprintf(stderr, "%s\n", line);
				// Request to server
				rc = ktp_sync_cmd(ktp, line, &retcode,
					error, opts->dry_run);
				if (!rc)
					retcode = -1;
				if (faux_error_len(error) > 0) {
					fprintf(stderr, "Error:\n");
					faux_error_fshow(error, stderr);
				}
				faux_error_free(error);
//				fprintf(stderr, "Retcode: %d\n", retcode);
				faux_str_free(line);
				// Stop-on-error
				if (opts->stop_on_error && (!rc || retcode != 0)) {
					stop = BOOL_TRUE;
					break;
				}
			}
			faux_file_close(fd);
			if (stop)
				break;
		}

	// Interactive shell
	} else {
		// Interactive code is complex so move it to separate file
		retcode = klish_interactive_shell(ktp, opts);
	}

	retval = 0;
err:
	ktp_session_free(ktp);
	faux_eloop_free(eloop);
	ktp_disconnect(unix_sock);
	opts_free(opts);

	if ((retval < 0) || (retcode < 0))
		return -1;

	return 0;
}


static bool_t ktp_sync_cmd(ktp_session_t *ktp, const char *line,
	int *retcode, faux_error_t *error, bool_t dry_run)
{
	if (!ktp_session_cmd(ktp, line, error, dry_run))
		return BOOL_FALSE;

	faux_eloop_loop(ktp_session_eloop(ktp));

	return ktp_session_retcode(ktp, retcode);
}


static bool_t stdout_cb(ktp_session_t *ktp, const char *line, size_t len,
	void *user_data)
{
	if (faux_write_block(STDOUT_FILENO, line, len) < 0)
		return BOOL_FALSE;

	ktp = ktp;
	user_data = user_data;

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


static bool_t stop_loop_ev(faux_eloop_t *eloop, faux_eloop_type_e type,
	void *associated_data, void *user_data)
{
	// Happy compiler
	eloop = eloop;
	type = type;
	associated_data = associated_data;
	user_data = user_data;

	return BOOL_FALSE; // Stop Event Loop
}
