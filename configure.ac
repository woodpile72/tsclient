AC_INIT([tsclient], [3.4],
        [http://www.gnomepro.com/tsclient])

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

AC_PROG_INTLTOOL(0.35.0)

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "\${ACLOCAL_FLAGS}")

GETTEXT_PACKAGE=tsclient
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [gettext package])

dnl Add the languages which your application supports here.
ALL_LINGUAS="ar ca cs da de el es fi fr gr he hu id it ja mk ms nl_NL nb pa pl_PL pt_BR pt_PT ru sv tr xh zh_CN zh_HK zh_TW"
AM_GLIB_GNU_GETTEXT

dnl ==========================================================================

set_debug=no
AC_ARG_ENABLE(debug,
[  --enable-debug       Lots of noisy debug messages],
set_debug="$enableval")

AC_MSG_CHECKING(if debug messages are enabled)
if test "$set_debug" != "no"; then
        AC_MSG_RESULT(yes)
        CFLAGS="-DTSCLIENT_DEBUG -g -O0"
else
        AC_MSG_RESULT(no)
fi

AC_ARG_ENABLE(gnome,
	AC_HELP_STRING([--disable-gnome],[Compile a gtk+ only version]),
	[case "${enableval}" in
	yes) ENABLE_GNOME=yes ;;
	no) ENABLE_GNOME=no ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --disable-gnome) ;;
	esac],
	[ENABLE_GNOME=yes])

# Gtk4 removed the historical panel applet APIs that tsclient used.
# Keep the flag for compatibility but ignore it in practice.
if test x$ENABLE_GNOME = "xno" ; then
   :
fi

pkg_modules="gtk4"

PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)


dnl ==========================================================================

AC_OUTPUT([
Makefile
src/Makefile
po/Makefile.in
])

echo "
$GETTEXT_PACKAGE-$VERSION:
        prefix:             ${prefix}
        source location:    ${srcdir}
        compiler:           ${CC}
	gnome support:      removed (gtk4)
        debug messages:     ${set_debug}

        Good luck!
"
