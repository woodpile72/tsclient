## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = tsclient

tsclient_SOURCES = \
	main.c main.h \
	support.c support.h \
	freerdp.c freerdp.h \
	connect.c connect.h \
	rdpfile.c rdpfile.h \
	mrulist.c mrulist.h \
	tsc-presets.h gtkcompat.h

tsclient_LDADD = @PACKAGE_LIBS@

check_PROGRAMS = test-freerdp
TESTS = test-freerdp
test_freerdp_SOURCES = ../tests/test-freerdp.c freerdp.c freerdp.h rdpfile.h
test_freerdp_CPPFLAGS = $(AM_CPPFLAGS) $(INCLUDES) -I$(srcdir)
test_freerdp_LDADD = @PACKAGE_LIBS@

EXTRA_DIST = ../tests/test-radio-groups.c
