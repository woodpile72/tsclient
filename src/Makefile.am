## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = tsclient

tsclient_SOURCES = \
	main.c main.h \
	support.c support.h \
	connect.c connect.h \
	rdpfile.c rdpfile.h \
	mrulist.c mrulist.h \
	tsc-presets.h

tsclient_LDADD = @PACKAGE_LIBS@
