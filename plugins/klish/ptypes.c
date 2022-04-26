/*
 * Implementation of standard PTYPEs
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <faux/str.h>
#include <faux/list.h>
#include <faux/conv.h>
#include <faux/argv.h>
#include <klish/kcontext.h>
#include <klish/kentry.h>


/** @brief PTYPE: Consider ENTRY's name (or "value" field) as a command
 *
 */
int klish_ptype_COMMAND(kcontext_t *context)
{
	const kentry_t *entry = NULL;
	const char *value = NULL;
	const char *command_name = NULL;

	entry = kcontext_candidate_entry(context);
	value = kcontext_candidate_value(context);

	command_name = kentry_value(entry);
	if (!command_name)
		command_name = kentry_name(entry);
	if (!command_name)
		return -1;

	return faux_str_casecmp(value, command_name);
}


/** @brief PTYPE: ENTRY's name (or "value" field) as a case sensitive command
 *
 */
int klish_ptype_COMMAND_CASE(kcontext_t *context)
{
	const kentry_t *entry = NULL;
	const char *value = NULL;
	const char *command_name = NULL;

	entry = kcontext_candidate_entry(context);
	value = kcontext_candidate_value(context);

	command_name = kentry_value(entry);
	if (!command_name)
		command_name = kentry_name(entry);
	if (!command_name)
		return -1;

	return strcmp(value, command_name);
}


/** @brief Signed int with optional range
 *
 * Use long long int for conversion from text.
 *
 * <ACTION sym="INT">-30 80</ACTION>
 * Means range from "-30" to "80"
 */
int klish_ptype_INT(kcontext_t *context)
{
	const char *script = NULL;
	const char *value_str = NULL;
	long long int value = 0;

	script = kcontext_script(context);
	value_str = kcontext_candidate_value(context);

	if (!faux_conv_atoll(value_str, &value, 0))
		return -1;

	// Range is specified
	if (!faux_str_is_empty(script)) {
		faux_argv_t *argv = faux_argv_new();
		const char *str = NULL;
		faux_argv_parse(argv, script);

		// Min
		str = faux_argv_index(argv, 0);
		if (str) {
			long long int min = 0;
			if (!faux_conv_atoll(str, &min, 0) || (value < min)) {
				faux_argv_free(argv);
				return -1;
			}
		}

		// Max
		str = faux_argv_index(argv, 1);
		if (str) {
			long long int max = 0;
			if (!faux_conv_atoll(str, &max, 0) || (value > max)) {
				faux_argv_free(argv);
				return -1;
			}
		}

		faux_argv_free(argv);
	}

	return 0;
}


/** @brief Unsigned int with optional range
 *
 * Use unsigned long long int for conversion from text.
 *
 * <ACTION sym="UINT">30 80</ACTION>
 * Means range from "30" to "80"
 */
int klish_ptype_UINT(kcontext_t *context)
{
	const char *script = NULL;
	const char *value_str = NULL;
	unsigned long long int value = 0;

	script = kcontext_script(context);
	value_str = kcontext_candidate_value(context);

	if (!faux_conv_atoull(value_str, &value, 0))
		return -1;

	// Range is specified
	if (!faux_str_is_empty(script)) {
		faux_argv_t *argv = faux_argv_new();
		const char *str = NULL;
		faux_argv_parse(argv, script);

		// Min
		str = faux_argv_index(argv, 0);
		if (str) {
			unsigned long long int min = 0;
			if (!faux_conv_atoull(str, &min, 0) || (value < min)) {
				faux_argv_free(argv);
				return -1;
			}
		}

		// Max
		str = faux_argv_index(argv, 1);
		if (str) {
			unsigned long long int max = 0;
			if (!faux_conv_atoull(str, &max, 0) || (value > max)) {
				faux_argv_free(argv);
				return -1;
			}
		}

		faux_argv_free(argv);
	}

	return 0;
}
