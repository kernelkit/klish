/* It must be here to include config.h before another headers */
#include "lub/db.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <lub/conv.h>

#define DEFAULT_GETPW_R_SIZE_MAX 1024

// Workaround to implement fully statical build. The libc uses dlopen() to
// implement NSS functions. So getpwnam() etc. need shared objects support.
// Exclude them on static build. Application will support UIDs and GIDs in
// digital format only.
#ifndef DISABLE_NSS

struct passwd *lub_db_getpwnam(const char *name)
{
	long int size;
	char *buf;
	struct passwd *pwbuf;
	struct passwd *pw = NULL;
	int res = 0;

#ifdef _SC_GETPW_R_SIZE_MAX
	if ((size = sysconf(_SC_GETPW_R_SIZE_MAX)) < 0)
		size = DEFAULT_GETPW_R_SIZE_MAX;
#else
	size = DEFAULT_GETPW_R_SIZE_MAX;
#endif
	pwbuf = malloc(sizeof(*pwbuf) + size);
	if (!pwbuf)
		return NULL;
	buf = (char *)pwbuf + sizeof(*pwbuf);
	
	res = getpwnam_r(name, pwbuf, buf, size, &pw);

	if (res || !pw) {
		free(pwbuf);
		if (res != 0)
			errno = res;
		else
			errno = ENOENT;
		return NULL;
	}

	return pwbuf;
}

struct passwd *lub_db_getpwuid(uid_t uid)
{
	long int size;
	char *buf;
	struct passwd *pwbuf;
	struct passwd *pw = NULL;
	int res = 0;

#ifdef _SC_GETPW_R_SIZE_MAX
	if ((size = sysconf(_SC_GETPW_R_SIZE_MAX)) < 0)
		size = DEFAULT_GETPW_R_SIZE_MAX;
#else
	size = DEFAULT_GETPW_R_SIZE_MAX;
#endif
	pwbuf = malloc(sizeof(*pwbuf) + size);
	if (!pwbuf)
		return NULL;
	buf = (char *)pwbuf + sizeof(*pwbuf);
	
	res = getpwuid_r(uid, pwbuf, buf, size, &pw);

	if (NULL == pw) {
		free(pwbuf);
		if (res != 0)
			errno = res;
		else
			errno = ENOENT;
		return NULL;
	}

	return pwbuf;
}

struct group *lub_db_getgrnam(const char *name)
{
	long int size;
	char *buf;
	struct group *grbuf;
	struct group *gr = NULL;
	int res = 0;

#ifdef _SC_GETGR_R_SIZE_MAX
	if ((size = sysconf(_SC_GETGR_R_SIZE_MAX)) < 0)
		size = DEFAULT_GETPW_R_SIZE_MAX;
#else
	size = DEFAULT_GETPW_R_SIZE_MAX;
#endif
	grbuf = malloc(sizeof(*grbuf) + size);
	if (!grbuf)
		return NULL;
	buf = (char *)grbuf + sizeof(*grbuf);
	
	res = getgrnam_r(name, grbuf, buf, size, &gr);

	if (!gr) {
		free(grbuf);
		if (res != 0)
			errno = res;
		else
			errno = ENOENT;
		return NULL;
	}

	return grbuf;
}

struct group *lub_db_getgrgid(gid_t gid)
{
	long int size;
	char *buf;
	struct group *grbuf;
	struct group *gr = NULL;
	int res = 0;

#ifdef _SC_GETGR_R_SIZE_MAX
	if ((size = sysconf(_SC_GETGR_R_SIZE_MAX)) < 0)
		size = DEFAULT_GETPW_R_SIZE_MAX;
#else
	size = DEFAULT_GETPW_R_SIZE_MAX;
#endif
	grbuf = malloc(sizeof(struct group) + size);
	if (!grbuf)
		return NULL;
	buf = (char *)grbuf + sizeof(struct group);

	res = getgrgid_r(gid, grbuf, buf, size, &gr);

	if (!gr) {
		free(grbuf);
		if (res != 0)
			errno = res;
		else
			errno = ENOENT;
		return NULL;
	}

	return grbuf;
}

// -----------------------------------------------------------------------------
#else // DISABLE_NSS

// All UIDs and GIDs must be in digital format. Functions only converts digits
// to text or text to digits. Text names are not supported. So functions
// simulate the real functions.

struct passwd *lub_db_getpwnam(const char *name)
{
	long int size = 0;
	struct passwd *buf = NULL;
	long int val = 0;

	assert(name);
	if (!name)
		return NULL;
	size = strlen(name) + 1;

	// Try to convert
	if (lub_conv_atol(name, &val, 0) < 0)
		return NULL;

	buf = calloc(1, sizeof(*buf) + size);
	if (!buf)
		return NULL;
	buf->pw_name = (char *)buf + sizeof(*buf);
	memcpy(buf->pw_name, name, size);
	buf->pw_uid = (uid_t)val;
	buf->pw_gid = (gid_t)val; // Let's primary GID be the same as UID

	return buf;
}

struct passwd *lub_db_getpwuid(uid_t uid)
{
	char str[20];
	long int size = 0;
	struct passwd *buf = NULL;

	snprintf(str, sizeof(str), "%u", uid);
	str[sizeof(str) - 1] = '\0';
	size = strlen(str) + 1;

	buf = calloc(1, sizeof(*buf) + size);
	if (!buf)
		return NULL;
	buf->pw_name = (char *)buf + sizeof(*buf);
	memcpy(buf->pw_name, str, size);
	buf->pw_uid = uid;
	buf->pw_gid = uid; // Let's primary GID be the same as UID

	return buf;
}

struct group *lub_db_getgrnam(const char *name)
{
	long int size = 0;
	struct group *buf = NULL;
	long int val = 0;

	assert(name);
	if (!name)
		return NULL;
	size = strlen(name) + 1;

	// Try to convert
	if (lub_conv_atol(name, &val, 0) < 0)
		return NULL;

	buf = calloc(1, sizeof(*buf) + size);
	if (!buf)
		return NULL;
	buf->gr_name = (char *)buf + sizeof(*buf);
	memcpy(buf->gr_name, name, size);
	buf->gr_gid = (gid_t)val;

	return buf;
}

struct group *lub_db_getgrgid(gid_t gid)
{
	char str[20];
	long int size = 0;
	struct group *buf = NULL;

	snprintf(str, sizeof(str), "%u", gid);
	str[sizeof(str) - 1] = '\0';
	size = strlen(str) + 1;

	buf = calloc(1, sizeof(*buf) + size);
	if (!buf)
		return NULL;
	buf->gr_name = (char *)buf + sizeof(*buf);
	memcpy(buf->gr_name, str, size);
	buf->gr_gid = gid;

	return buf;
}

#endif // DISABLE_NSS
