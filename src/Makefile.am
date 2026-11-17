## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = tsclient

common_sources = \
	support.c support.h \
	freerdp.c freerdp.h \
	connect.c connect.h connection.c connection.h \
	launcher.c launcher.h session.c session.h \
	credentials.c credentials.h profile.c \
	rdpfile.c rdpfile.h \
	mrulist.c mrulist.h \
	tsc-presets.h

tsclient_SOURCES = main.c main.h $(common_sources)

tsclient_LDADD = @PACKAGE_LIBS@

check_PROGRAMS = test-freerdp test-profile test-launcher test-session test-ui test-credentials test-mru test-connection-credentials
TESTS = $(check_PROGRAMS)
test_freerdp_SOURCES = ../tests/test-freerdp.c freerdp.c freerdp.h rdpfile.h
test_freerdp_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
test_freerdp_LDADD = @PACKAGE_LIBS@



test_profile_SOURCES = ../tests/test-profile.c profile.c rdpfile.h
test_profile_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
test_profile_LDADD = @PACKAGE_LIBS@
test_launcher_SOURCES = ../tests/test-launcher.c launcher.c launcher.h freerdp.c freerdp.h rdpfile.h
test_launcher_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
test_launcher_LDADD = @PACKAGE_LIBS@
test_session_SOURCES = ../tests/test-session.c session.c session.h
test_session_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
test_session_LDADD = @PACKAGE_LIBS@
test_ui_SOURCES = ../tests/test-ui.c $(common_sources)
test_ui_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir) -DTEST_ICON_DIR=\""$(abs_srcdir)/../icons"\"
test_ui_LDADD = @PACKAGE_LIBS@

test_credentials_SOURCES = ../tests/test-credentials.c credentials.c credentials.h rdpfile.h
test_credentials_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
# All Secret Service functions are mocked; this test never opens a real keyring.
test_credentials_LDADD = @CORE_LIBS@

test_mru_SOURCES = ../tests/test-profile.c profile.c rdpfile.h
test_mru_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir) -DTEST_MRU
test_mru_CFLAGS = $(AM_CFLAGS) -ffunction-sections -fdata-sections
test_mru_LDFLAGS = -Wl,--gc-sections
test_mru_LDADD = @PACKAGE_LIBS@

# connection.c is included by the test so its static callbacks can be exercised.
# Do not compile it separately or link production credential/session functions.
test_connection_credentials_SOURCES = ../tests/test-connection-credentials.c
test_connection_credentials_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
test_connection_credentials_LDADD = $(filter-out -lsecret-1,@PACKAGE_LIBS@)

# Automake cannot infer prerequisites from GNU make function expressions.
test_connection_credentials_DEPENDENCIES =
