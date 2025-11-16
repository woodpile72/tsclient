## Process this file with automake to produce Makefile.in

pixmapsdir = $(datadir)/tsclient/icons

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

libexec_PROGRAMS = tsclient-applet

tsclient_applet_SOURCES = \
	applet.c applet.h \
	../src/support.c ../src/support.h \
	../src/rdpfile.c ../src/rdpfile.h \
	../src/mrulist.c ../src/mrulist.h

tsclient_applet_LDADD = @PACKAGE_LIBS@

server_in_files = GNOME_TSClientApplet.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" -e "s|\@PIXMAPSDIR\@|$(pixmapsdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files)
CLEANFILES = $(server_DATA) $(server_DATA).in
