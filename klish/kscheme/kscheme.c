#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <faux/str.h>
#include <faux/list.h>
#include <klish/khelper.h>
#include <klish/kplugin.h>
#include <klish/kptype.h>
#include <klish/kview.h>
#include <klish/kscheme.h>


struct kscheme_s {
	faux_list_t *plugins;
	faux_list_t *ptypes;
	faux_list_t *views;
};

// Simple methods

// PLUGIN list
KCMP_NESTED(scheme, plugin, name);
KCMP_NESTED_BY_KEY(scheme, plugin, name);
KADD_NESTED(scheme, plugin);
KFIND_NESTED(scheme, plugin);
KNESTED_LEN(scheme, plugin);
KNESTED_ITER(scheme, plugin);
KNESTED_EACH(scheme, plugin);

// PTYPE list
KCMP_NESTED(scheme, ptype, name);
KCMP_NESTED_BY_KEY(scheme, ptype, name);
KADD_NESTED(scheme, ptype);
KFIND_NESTED(scheme, ptype);
KNESTED_LEN(scheme, ptype);
KNESTED_ITER(scheme, ptype);
KNESTED_EACH(scheme, ptype);

// VIEW list
KCMP_NESTED(scheme, view, name);
KCMP_NESTED_BY_KEY(scheme, view, name);
KADD_NESTED(scheme, view);
KFIND_NESTED(scheme, view);
KNESTED_LEN(scheme, view);
KNESTED_ITER(scheme, view);
KNESTED_EACH(scheme, view);


kscheme_t *kscheme_new(void)
{
	kscheme_t *scheme = NULL;

	scheme = faux_zmalloc(sizeof(*scheme));
	assert(scheme);
	if (!scheme)
		return NULL;

	// PLUGIN list
	scheme->plugins = faux_list_new(FAUX_LIST_SORTED, FAUX_LIST_UNIQUE,
		kscheme_plugin_compare, kscheme_plugin_kcompare,
		(void (*)(void *))kplugin_free);
	assert(scheme->plugins);

	// PTYPE list
	scheme->ptypes = faux_list_new(FAUX_LIST_SORTED, FAUX_LIST_UNIQUE,
		kscheme_ptype_compare, kscheme_ptype_kcompare,
		(void (*)(void *))kptype_free);
	assert(scheme->ptypes);

	// VIEW list
	scheme->views = faux_list_new(FAUX_LIST_SORTED, FAUX_LIST_UNIQUE,
		kscheme_view_compare, kscheme_view_kcompare,
		(void (*)(void *))kview_free);
	assert(scheme->views);

	return scheme;
}


void kscheme_free(kscheme_t *scheme)
{
	if (!scheme)
		return;

	faux_list_free(scheme->plugins);
	faux_list_free(scheme->ptypes);
	faux_list_free(scheme->views);
	faux_free(scheme);
}

#if 0
/** @brief Prepares schema for execution.
 *
 * It loads plugins, link unresolved symbols, then iterates all the
 * objects and link them to each other, check access
 * permissions. Without this function the schema is not fully functional.
 */
int kscheme_prepare(kscheme_t *scheme)
{
	clish_command_t *cmd;
	clish_view_t *view;
	clish_nspace_t *nspace;
	lub_list_t *view_tree, *nspace_tree;
	lub_list_node_t *nspace_iter, *view_iter;
	lub_bintree_t *cmd_tree;
	lub_bintree_iterator_t cmd_iter;
	clish_hook_access_fn_t *access_fn = NULL;
	clish_paramv_t *paramv;
	int i = 0;

	/* Add statically linked plugins */
	while (clish_plugin_builtin_list[i].name) {
		clish_plugin_t *plugin;
		plugin = clish_shell_find_create_plugin(this,
			clish_plugin_builtin_list[i].name);
		clish_plugin__set_init(plugin,
			clish_plugin_builtin_list[i].init);
		clish_plugin__set_builtin_flag(plugin, BOOL_TRUE);
		i++;
	}

	/* Add default plugin to the list of plugins */
	if (this->default_plugin) {
		clish_shell_find_create_plugin(this, "clish");
		/* Default hooks */
		for (i = 0; i < CLISH_SYM_TYPE_MAX; i++) {
			if (this->hooks_use[i])
				continue;
			if (!clish_plugin_default_hook[i])
				continue;
			clish_sym__set_name(this->hooks[i],
				clish_plugin_default_hook[i]);
		}
	}
	/* Add default syms to unresolved table */
	for (i = 0; i < CLISH_SYM_TYPE_MAX; i++) {
		if (clish_sym__get_name(this->hooks[i]))
			lub_list_add(this->syms, this->hooks[i]);
	}

	/* Load plugins and link symbols */
	if (clish_shell_load_plugins(this) < 0)
		return -1;
	if (clish_shell_link_plugins(this) < 0)
		return -1;

	access_fn = clish_sym__get_func(clish_shell_get_hook(this, CLISH_SYM_TYPE_ACCESS));

	/* Iterate the VIEWs */
	view_tree = this->view_tree;
	view_iter = lub_list_iterator_init(view_tree);
	while(view_iter) {
		lub_list_node_t *old_view_iter;
		view = (clish_view_t *)lub_list_node__get_data(view_iter);
		old_view_iter = view_iter;
		view_iter = lub_list_node__get_next(view_iter);
		/* Check access rights for the VIEW */
		if (access_fn && clish_view__get_access(view) &&
			access_fn(this, clish_view__get_access(view))) {
#ifdef DEBUG
			fprintf(stderr, "Warning: Access denied. Remove VIEW \"%s\"\n",
				clish_view__get_name(view));
#endif
			lub_list_del(view_tree, old_view_iter);
			lub_list_node_free(old_view_iter);
			clish_view_delete(view);
			continue;
		}

		/* Iterate the NAMESPACEs */
		nspace_tree = clish_view__get_nspaces(view);
		nspace_iter = lub_list__get_head(nspace_tree);
		while(nspace_iter) {
			clish_view_t *ref_view;
			lub_list_node_t *old_nspace_iter;
			nspace = (clish_nspace_t *)lub_list_node__get_data(nspace_iter);
			old_nspace_iter = nspace_iter;
			nspace_iter = lub_list_node__get_next(nspace_iter);
			/* Resolve NAMESPACEs and remove unresolved ones */
			ref_view = clish_shell_find_view(this, clish_nspace__get_view_name(nspace));
			if (!ref_view) {
#ifdef DEBUG
				fprintf(stderr, "Warning: Remove unresolved NAMESPACE \"%s\" from \"%s\" VIEW\n",
					clish_nspace__get_view_name(nspace), clish_view__get_name(view));
#endif
				lub_list_del(nspace_tree, old_nspace_iter);
				lub_list_node_free(old_nspace_iter);
				clish_nspace_delete(nspace);
				continue;
			}
			clish_nspace__set_view(nspace, ref_view);
			clish_nspace__set_view_name(nspace, NULL); /* Free some memory */
			/* Check access rights for the NAMESPACE */
			if (access_fn && (
				/* Check NAMESPASE owned access */
				(clish_nspace__get_access(nspace) && access_fn(this, clish_nspace__get_access(nspace)))
				||
				/* Check referenced VIEW's access */
				(clish_view__get_access(ref_view) && access_fn(this, clish_view__get_access(ref_view)))
				)) {
#ifdef DEBUG
				fprintf(stderr, "Warning: Access denied. Remove NAMESPACE \"%s\" from \"%s\" VIEW\n",
					clish_nspace__get_view_name(nspace), clish_view__get_name(view));
#endif
				lub_list_del(nspace_tree, old_nspace_iter);
				lub_list_node_free(old_nspace_iter);
				clish_nspace_delete(nspace);
				continue;
			}
		}

		/* Iterate the COMMANDs */
		cmd_tree = clish_view__get_tree(view);
		cmd = lub_bintree_findfirst(cmd_tree);
		for (lub_bintree_iterator_init(&cmd_iter, cmd_tree, cmd);
			cmd; cmd = lub_bintree_iterator_next(&cmd_iter)) {
			int cmd_is_alias = clish_command__get_alias(cmd)?1:0;
			clish_param_t *args = NULL;

			/* Check access rights for the COMMAND */
			if (access_fn && clish_command__get_access(cmd) &&
				access_fn(this, clish_command__get_access(cmd))) {
#ifdef DEBUG
				fprintf(stderr, "Warning: Access denied. Remove COMMAND \"%s\" from VIEW \"%s\"\n",
					clish_command__get_name(cmd), clish_view__get_name(view));
#endif
				lub_bintree_remove(cmd_tree, cmd);
				clish_command_delete(cmd);
				continue;
			}

			/* Resolve command aliases */
			if (cmd_is_alias) {
				clish_view_t *aview;
				clish_command_t *cmdref;
				const char *alias_view = clish_command__get_alias_view(cmd);
				if (!alias_view)
					aview = clish_command__get_pview(cmd);
				else
					aview = clish_shell_find_view(this, alias_view);
				if (!aview /* Removed or broken VIEW */
					||
					/* Removed or broken referenced COMMAND */
					!(cmdref = clish_view_find_command(aview, clish_command__get_alias(cmd), BOOL_FALSE))
					) {
#ifdef DEBUG
					fprintf(stderr, "Warning: Remove unresolved link \"%s\" from \"%s\" VIEW\n",
						clish_command__get_name(cmd), clish_view__get_name(view));
#endif
					lub_bintree_remove(cmd_tree, cmd);
					clish_command_delete(cmd);
					continue;
					/*fprintf(stderr, CLISH_XML_ERROR_STR"Broken VIEW for alias \"%s\"\n",
						clish_command__get_name(cmd));
					return -1; */
					/*fprintf(stderr, CLISH_XML_ERROR_STR"Broken alias \"%s\"\n",
						clish_command__get_name(cmd));
					return -1; */
				}
				if (!clish_command_alias_to_link(cmd, cmdref)) {
					fprintf(stderr, CLISH_XML_ERROR_STR"Something wrong with alias \"%s\"\n",
						clish_command__get_name(cmd));
					return -1;
				}
				/* Check access rights for newly constructed COMMAND.
				   Now the link has access filed from referenced command.
				 */
				if (access_fn && clish_command__get_access(cmd) &&
					access_fn(this, clish_command__get_access(cmd))) {
#ifdef DEBUG
					fprintf(stderr, "Warning: Access denied. Remove COMMAND \"%s\" from VIEW \"%s\"\n",
						clish_command__get_name(cmd), clish_view__get_name(view));
#endif
					lub_bintree_remove(cmd_tree, cmd);
					clish_command_delete(cmd);
					continue;
				}
			}
			if (cmd_is_alias) /* Don't duplicate paramv processing for aliases */
				continue;
			/* Iterate PARAMeters */
			paramv = clish_command__get_paramv(cmd);
			if (iterate_paramv(this, paramv, access_fn) < 0)
				return -1;
			/* Resolve PTYPE for args */
			if ((args = clish_command__get_args(cmd))) {
				if (!resolve_ptype(this, args))
					return -1;
			}
		}
	}

	return 0;
}
#endif
