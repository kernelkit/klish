/*
 * shell_misc.c
 */

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>

#include "private.h"

CLISH_GET_STR(shell, overview);

bool_t clish_shell_is_machine_interface(const clish_shell_t *shell)
{
	assert(shell);
	if (!shell)
		return BOOL_FALSE;

	return shell->machine_interface;
}


void clish_shell_set_machine_interface(clish_shell_t *shell)
{
	assert(shell);
	if (!shell)
		return;

	shell->machine_interface = BOOL_TRUE;
	if (shell->tinyrl)
		tinyrl_set_machine_interface(shell->tinyrl);
}


void clish_shell_set_human_interface(clish_shell_t *shell)
{
	assert(shell);
	if (!shell)
		return;

	shell->machine_interface = BOOL_FALSE;
	if (shell->tinyrl)
		tinyrl_set_human_interface(shell->tinyrl);
}


/* Get user name.
 * Return value must be freed after using.
 */
char *clish_shell_format_username(const clish_shell_t *shell)
{
	char *uname = NULL;

	/* Try to get username from environment variables
	 * USER and LOGNAME and then from /etc/passwd. Else use UID.
	 */
	if (!(uname = getenv("USER"))) {
		if (!(uname = getenv("LOGNAME"))) {
			struct passwd *user = NULL;
			user = clish_shell__get_user(shell);
			if (user) {
				uname = user->pw_name;
			} else {
				char tmp[10] = {};
				snprintf(tmp, sizeof(tmp), "%u", getuid());
				tmp[sizeof(tmp) - 1] = '\0';
				uname = tmp;
			}
		}
	}

	return strdup(uname);
}
