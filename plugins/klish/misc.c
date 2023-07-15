/*
 *
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <faux/str.h>
#include <faux/list.h>
#include <faux/sysdb.h>
#include <klish/kcontext.h>
#include <klish/ksession.h>
#include <klish/kpath.h>


int klish_nop(kcontext_t *context)
{
	context = context; // Happy compiler

	return 0;
}


// Symbol for testing purposes
int klish_tsym(kcontext_t *context)
{
	const char *script = NULL;

	script = kcontext_script(context);
	if (faux_str_is_empty(script)) {
		printf("[<empty>]\n");
		fprintf(stderr, "Empty item\n");
		return -1;
	}

	printf("[%s]\n", script);

	return 0;
}


// Print content of ACTION script
int klish_printl(kcontext_t *context)
{
	const char *script = NULL;

	script = kcontext_script(context);
	if (faux_str_is_empty(script))
		script = "";

	printf("%s\n", script);

	return 0;
}


// Print content of ACTION script. Without additional '/n'
int klish_print(kcontext_t *context)
{
	const char *script = NULL;

	script = kcontext_script(context);
	if (faux_str_is_empty(script))
		script = "";

	printf("%s", script);
	fflush(stdout);

	return 0;
}

static char *cwd(kcontext_t *context)
{
	kpath_levels_node_t *iter;
	static char buf[256];
	ksession_t *session;
	klevel_t *level;
	size_t len;

	len = sizeof(buf);
	memset(buf, 0, len);

	session = kcontext_session(context);
	if (!session)
		return NULL;

	iter = kpath_iter(ksession_path(session));
	while ((level = kpath_each(&iter))) {
		const char *nm = kentry_name(klevel_entry(level));

		strncat(buf, "/", len--);
		strncat(buf, nm, len);
		len -= strlen(nm);
	}

	return buf;
}

// Symbol to show current path
int klish_pwd(kcontext_t *context)
{
	printf("%s\n", cwd(context));
	return 0;
}


// Template for easy prompt string generation
int klish_prompt(kcontext_t *context)
{
	const char *script = NULL;
	const char *start = NULL;
	const char *pos = NULL;
	char *prompt = NULL;
	bool_t is_macro = BOOL_FALSE;

	script = kcontext_script(context);
	if (faux_str_is_empty(script))
		return 0;
	pos = script;
	start = script;

	while (*pos != '\0') {

		if (is_macro) {
			switch (*pos) {
			// Percent symbol itself
			case '%': {
				faux_str_cat(&prompt, "%");
				break;
				}
			// Hostname
			case 'h': {
				struct utsname buf;
				if (uname(&buf) == 0)
					faux_str_cat(&prompt, buf.nodename);
				break;
				}
			// Username
			case 'u': {
				char *user = getenv("USER");
				if (user) {
					faux_str_cat(&prompt, user);
					break;
				}
				user = faux_sysdb_name_by_uid(getuid());
				if (user)
					faux_str_cat(&prompt, user);
				faux_str_free(user);
				break;
				}
			// Workdir (pwd)
			case 'w':
				faux_str_cat(&prompt, cwd(context));
				break;
			}
			is_macro = BOOL_FALSE;
			start = pos + 1;
		} else if (*pos == '%') {
			is_macro = BOOL_TRUE;
			if (pos > start)
				faux_str_catn(&prompt, start, pos - start);
		}
		pos++;
	}
	if (pos > start)
		faux_str_catn(&prompt, start, pos - start);

	printf("%s", prompt);
	faux_str_free(prompt);
	fflush(stdout);

	return 0;
}
