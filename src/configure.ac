AC_INIT([screen], [4.99.0-devel], [https://savannah.gnu.org/bugs/?group=screen], [screen], [https://www.gnu.org/software/screen/])
AC_PREREQ(2.60)

AC_CONFIG_HEADERS([config.h])

AH_TEMPLATE([HAVE_EXECVPE], [system has execvpe() defined])
AH_TEMPLATE([HAVE_OPENPTY], [system has openpty() defined])

AH_TEMPLATE([ENABLE_PAM], [Enable Pluggable Authenticaion Modules support])
AH_TEMPLATE([ENABLE_UTMP], [Enable utmp support])
AH_TEMPLATE([ENABLE_TELNET], [Enable built in telnet client])

dnl
dnl Check for sane compiler
dnl

AC_GNU_SOURCE
AC_PROG_CC
AC_PROG_AWK
AC_PROG_INSTALL
AC_USE_SYSTEM_EXTENSIONS

dnl
dnl Check various system stuff
dnl

dnl check for execvpe()
AC_CHECK_FUNCS([execvpe])

dnl
AC_CHECK_FUNCS([seteuid setegid setresuid])

dnl curses compatible lib, we do forward declaration ourselves, only need to link to proper library
AC_SEARCH_LIBS([tgetent], [curses termcap termlib ncursesw tinfow ncurses tinfo], [], [
	AC_MSG_ERROR([unable to find tgetent() function])
])

dnl check for crypt()
AC_SEARCH_LIBS([crypt], [crypt], [], [
	AC_MSG_ERROR([unable to find crypt() function])
])

dnl
AC_CHECK_HEADERS(pty.h util.h libutil.h)
AC_SEARCH_LIBS([openpty], [util openpty], [
	AC_DEFINE(HAVE_OPENPTY)
	LIBS="$LIBS -lutil"
], [
	AC_MSG_ERROR([unable to find openpty() function])
])

dnl
dnl Check user changeable stuff
dnl

AC_ARG_ENABLE(pam, AS_HELP_STRING([--enable-pam], [enable PAM support (default: disabled)]), [enable_pam=$enableval], [enable_pam=no])
if test "$enable_pam" = "yes"; then
	AC_MSG_CHECKING(for PAM support)
	oldlibs="$LIBS"
	LIBS="$LIBS -lpam"
	AC_TRY_LINK([#include <security/pam_appl.h>], [
		pam_start(0, 0, 0, 0);
		pam_authenticate(0, 0);
		pam_end(0,0);
], AC_MSG_RESULT(yes);
	AC_DEFINE(ENABLE_PAM),
	AC_MSG_RESULT(nr);LIBS="$oldlibs")
fi

AC_ARG_ENABLE(utmp, AS_HELP_STRING([--enable-utmp], [enable utmp support (default: disabled)]), [enable_utmp=$enableval], [enable_utmp=no])
if test "$enable_utmp" = "yes"; then
	AC_DEFINE(ENABLE_UTMP)
fi

AC_ARG_ENABLE(telnet, AS_HELP_STRING([--enable-telnet], [enable telnet support (default: disabled)]), [enable_telnet=$enableval], [enable_telnet=no])
if test "$enable_telnet" = "yes"; then
	AC_DEFINE(ENABLE_TELNET)
fi

dnl
dnl Generate files from *.in ones
dnl

AC_OUTPUT(Makefile doc/Makefile)
