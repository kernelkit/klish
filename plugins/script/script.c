/*
 *
 */

#include <assert.h>
#include <grp.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <syslog.h>

#include <faux/str.h>
#include <faux/list.h>
#include <klish/kcontext.h>
#include <klish/ksession.h>


const char *kcontext_type_e_str[] = {
	"none",
	"plugin_init",
	"plugin_fini",
	"action",
	"service_action"
	};

#define PREFIX "KLISH_"
#define OVERWRITE 1


static bool_t populate_env_kpargv(const kpargv_t *pargv, const char *prefix)
{
	const kentry_t *cmd = NULL;
	faux_list_node_t *iter = NULL;
	faux_list_node_t *cur = NULL;

	if (!pargv)
		return BOOL_FALSE;

	// Command
	cmd = kpargv_command(pargv);
	if (cmd) {
		char *var = faux_str_sprintf("%sCOMMAND", prefix);
		setenv(var, kentry_name(cmd), OVERWRITE);
		faux_str_free(var);
	}

	// Parameters
	iter = faux_list_head(kpargv_pargs(pargv));
	while ((cur = faux_list_each_node(&iter))) {
		kparg_t *parg = (kparg_t *)faux_list_data(cur);
		kparg_t *tmp_parg = NULL;
		const kentry_t *entry = kparg_entry(parg);
		faux_list_node_t *iter_before = faux_list_prev_node(cur);
		faux_list_node_t *iter_after = cur;
		unsigned int num = 0;
		bool_t already_populated = BOOL_FALSE;

		if (!kparg_value(parg)) // PTYPE can contain parg with NULL value
			continue;

		// Search for such entry within arguments before current one
		while ((tmp_parg = (kparg_t *)faux_list_eachr(&iter_before))) {
			if (kparg_entry(tmp_parg) == entry) {
				already_populated = BOOL_TRUE;
				break;
			}
		}
		if (already_populated)
			continue;

		// Populate all next args with the current entry
		while ((tmp_parg = (kparg_t *)faux_list_each(&iter_after))) {
			char *var = NULL;
			const char *value = kparg_value(tmp_parg);
			if (kparg_entry(tmp_parg) != entry)
				continue;
			if (num == 0) {
				var = faux_str_sprintf("%sPARAM_%s",
					prefix, kentry_name(entry));
				setenv(var, value, OVERWRITE);
				faux_str_free(var);
			}
			var = faux_str_sprintf("%sPARAM_%s_%u",
				prefix, kentry_name(entry), num);
			setenv(var, value, OVERWRITE);
			faux_str_free(var);
			num++;
		}
	}

	return BOOL_TRUE;
}


static bool_t populate_env(kcontext_t *context)
{
	kcontext_type_e type = KCONTEXT_TYPE_NONE;
	const kentry_t *entry = NULL;
	const ksession_t *session = NULL;
	const char *str = NULL;
	pid_t pid = -1;
	uid_t uid = -1;

	assert(context);
	session = kcontext_session(context);
	assert(session);

	// Type
	type = kcontext_type(context);
	if (type >= KCONTEXT_TYPE_MAX)
		type = KCONTEXT_TYPE_NONE;
	setenv(PREFIX"TYPE", kcontext_type_e_str[type], OVERWRITE);

	// Candidate
	entry = kcontext_candidate_entry(context);
	if (entry)
		setenv(PREFIX"CANDIDATE", kentry_name(entry), OVERWRITE);

	// Value
	str = kcontext_candidate_value(context);
	if (str)
		setenv(PREFIX"VALUE", str, OVERWRITE);

	// PID
	pid = ksession_pid(session);
	if (pid != -1) {
		char *t = faux_str_sprintf("%lld", (long long int)pid);
		setenv(PREFIX"PID", t, OVERWRITE);
		faux_str_free(t);
	}

	// UID
	uid = ksession_uid(session);
	if (uid != -1) {
		char *t = faux_str_sprintf("%lld", (long long int)uid);
		setenv(PREFIX"UID", t, OVERWRITE);
		faux_str_free(t);
	}

	// User
	str = ksession_user(session);
	if (str) {
		setenv(PREFIX"USER", str, OVERWRITE);
		setenv("USER", str, OVERWRITE);
		setenv("LOGNAME", str, OVERWRITE);
	}

	// Parameters
	populate_env_kpargv(kcontext_pargv(context), PREFIX);

	// Parent parameters
	populate_env_kpargv(kcontext_parent_pargv(context), PREFIX"PARENT_");

	return BOOL_TRUE;
}


static char *find_out_shebang(const char *script)
{
	char *default_shebang = "/bin/sh";
	char *shebang = NULL;
	char *line = NULL;

	line = faux_str_getline(script, NULL);
	if (
		!line ||
		(strlen(line) < 2) ||
		(line[0] != '#') ||
		(line[1] != '!')
		) {
		faux_str_free(line);
		return faux_str_dup(default_shebang);
	}

	shebang = faux_str_dup(line + 2);
	faux_str_free(line);

	return shebang;
}


// Execute script
int script_script(kcontext_t *context)
{
	char script_name[] = "/tmp/klish.XXXXXX";
	const ksession_t *session = NULL;
	const char *script = NULL;
	char *command = NULL;
	char *shebang = NULL;
	pid_t cpid = -1;
	int status;
	uid_t uid;
	uid_t gid;
	FILE *fp;

	assert(context);
	session = kcontext_session(context);
	assert(session);

	script = kcontext_script(context);
	if (faux_str_is_empty(script))
		return 0;

	// Create script
	uid = ksession_uid(session);
	gid = ksession_gid(session);

	mktemp(script_name);

	// Parent: write to script
	fp = fopen(script_name, "w");
	if (!fp) {
		fprintf(stderr, "Error: Can't open script for writing.\n");
		unlink(script_name);
		faux_str_free(script_name);
		return -1;
	}

	// Prepare command
	shebang = find_out_shebang(script);
	fprintf(fp, "#!%s\n%s\n", shebang, script);
	fclose(fp);
	if (chown(script_name, uid, gid) || chmod(script_name, 0755))
		syslog(LOG_ERR, "Failed setting perms on %s: %s", script_name, strerror(errno));

	// Fork process
	cpid = fork();
	if (cpid == -1) {
		fprintf(stderr, "Error: failed forking off executor, error %d.\n"
			"Error: The ACTION will not be executed.\n", errno);
		unlink(script_name);
		faux_str_free(script_name);

		return -1;
	}

	// Child: drop privileges and execute command
	if (cpid == 0) {
		gid_t groups[NGROUPS_MAX];
		int ngroups = NGROUPS_MAX;
		int res;

		populate_env(context);
		setgroups(0, NULL);

		// Get supplementary groups
		if (getgrouplist(ksession_user(session), gid, groups, &ngroups) == -1) {
			syslog(LOG_ERR, "Failed retrieving supplementary groups: %s", strerror(errno));
			_exit(-1);
		}

		// Set supplementary groups
		if (setgroups(ngroups, groups) != 0) {
			syslog(LOG_ERR, "Failed setting supplementary groups: %s", strerror(errno));
			_exit(-1);
		}
		// Drop privileges
		if (setgid(gid) || setuid(uid)) {
			syslog(LOG_ERR, "Failed dropping privileges to (UID:%d GID:%d): %s",
			       uid, gid, strerror(errno));
			_exit(-1);
		}

		dup2(STDOUT_FILENO, STDERR_FILENO);

		// Execute command
		res = system(script_name);

		// Clean up
		faux_str_free(command);
		faux_str_free(shebang);

		_exit(WEXITSTATUS(res));
	}

	// Wait for the child process
	while (waitpid(cpid, &status, 0) != cpid)
		;

	// Clean up
	unlink(script_name);

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	return -1;
}
