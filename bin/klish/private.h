#include <faux/list.h>
#include <klish/ktp.h>
#include <klish/ktp_session.h>

#ifndef VERSION
#define VERSION "1.0.0"
#endif

#define DEFAULT_CFGFILE "/etc/klish/klish.conf"
#define DEFAULT_PAGER "/usr/bin/less -I -F -e -X -K -d -R"

#define OBUF_LIMIT 65536

/** @brief Command line and config file options
 */
struct options {
	bool_t verbose;
	char *cfgfile;
	bool_t cfgfile_userdefined;
	char *unix_socket_path;
	char *pager;
	char *motd;
	bool_t pager_enabled;
	bool_t hist_save_always;
	size_t hist_size;
	bool_t stop_on_error;
	bool_t dry_run;
	bool_t quiet;
	faux_list_t *commands;
	faux_list_t *files;
};

// Options
void help(int status, const char *argv0);
struct options *opts_init(void);
void opts_free(struct options *opts);
int opts_parse(int argc, char *argv[], struct options *opts);
bool_t config_parse(const char *cfgfile, struct options *opts);
