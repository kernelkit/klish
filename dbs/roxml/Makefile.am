db_LTLIBRARIES += kdb-roxml.la
kdb_roxml_la_SOURCES =
kdb_roxml_la_LDFLAGS = $(AM_LDFLAGS) @ROXML_LDFLAGS@ -avoid-version -module
kdb_roxml_la_CFLAGS = $(AM_LDFLAGS) @ROXML_CFLAGS@
kdb_roxml_la_LIBADD = libklish-helper-xml.la @ROXML_LIBS@

kdb_roxml_la_SOURCES += \
	dbs/roxml/roxml_api.c \
	dbs/roxml/roxml_plugin.c
