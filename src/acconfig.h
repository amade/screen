/* Copyright (c) 1993-2000
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, see
 * http://www.gnu.org/licenses/, or contact Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 ****************************************************************
 * $Id$ GNU
 */





/**********************************************************************
 *
 *	User Configuration Section
 */

/*
 * Maximum of simultaneously allowed windows per screen session.
 */
#ifndef MAXWIN
# define MAXWIN 100
#endif

/*
 * Screen can look for the environment variable $SYSSCREENRC and -if it
 * exists- load the file specified in that variable as global screenrc.
 * If you want to enable this feature, define ALLOW_SYSSCREENRC to one (1).
 * Otherwise ETCSCREENRC is always loaded.
 */
#define ALLOW_SYSSCREENRC 1

/*
 * If you'd rather see the status line on the first line of your
 * terminal rather than the last, define TOPSTAT.
 */
#undef TOPSTAT

/*
 * to lower the interrupt load on the host machine, you may want to
 * adjust the VMIN and VTIME settings used for plain tty windows.
 * See the termio(4) manual page (Non-Canonical Mode Input Processing)
 * for details.
 * if undefined, VMIN=1, VTIME=0 is used as a default - this gives you
 * best user responsiveness, but highest interrupt frequency.
 * (Do not bother, if you are not using plain tty windows.)
 */
#define TTYVMIN 100
#define TTYVTIME 2

/*
 * Some terminals, e.g. Wyse 120, use a bitfield to select attributes.
 * This doesn't work with the standard so/ul/m? terminal entries,
 * because they will cancel each other out.
 * On TERMINFO machines, "sa" (sgr) may work. If you want screen
 * to switch attributes only with sgr, define USE_SGR.
 * This is *not* recomended, do this only if you must.
 */
#undef USE_SGR


/*
 *
 */
#undef USE_PAM

/**********************************************************************
 *
 *	End of User Configuration Section
 *
 *      Rest of this file is modified by 'configure'
 *      Change at your own risk!
 *
 */

/*
 * Define POSIX if your system supports IEEE Std 1003.1-1988 (POSIX).
 */
#undef POSIX

/*
 * Define TERMIO if you have struct termio instead of struct sgttyb.
 * This is usually the case for SVID systems, where BSD uses sgttyb.
 * POSIX systems should define this anyway, even though they use
 * struct termios.
 */
#undef TERMIO

/*
 * Define CYTERMIO if you have cyrillic termio modes.
 */
#undef CYTERMIO

/*
 * If your library does not define ospeed, define this.
 */
#undef NEED_OSPEED

/*
 * Define SIGVOID if your signal handlers return void.  On older
 * systems, signal returns int, but on newer ones, it returns void.
 */
#undef SIGVOID

/*
 * Define USESIGSET if you have sigset for BSD 4.1 reliable signals.
 */
#undef USESIGSET

/*
 * If your system has getutent(), pututline(), etc. to write to the
 * utmp file, define GETUTENT.
 */
#undef GETUTENT

/*
 * Define UTHOST if the utmp file has a host field.
 */
#undef UTHOST

/*
 * Define if you have the utempter utmp helper program
 */
#undef HAVE_UTEMPTER

/*
 * If ttyslot() breaks getlogin() by returning indexes to utmp entries
 * of type DEAD_PROCESS, then our getlogin() replacement should be
 * selected by defining BUGGYGETLOGIN.
 */
#undef BUGGYGETLOGIN

/*
 * execvpe is now defined in some systems.
 */
#undef HAVE_EXECVPE

/*
 * If your system has the new format /etc/ttys (like 4.3 BSD) and the
 * getttyent(3) library functions, define GETTTYENT.
 */
#undef GETTTYENT

/*
 * If your system has vsprintf() and requires the use of the macros in
 * "varargs.h" to use functions with variable arguments,
 * define USEVARARGS.
 */
#undef USEVARARGS

/*
 * define HAVE_NL_LANGINFO if your system has the nl_langinfo() call
 * and <langinfo.h> defines CODESET.
 */
#undef HAVE_NL_LANGINFO

/*
 * define PTYRANGE0 and or PTYRANGE1 if you want to adapt screen
 * to unusual environments. E.g. For SunOs the defaults are "qpr" and
 * "0123456789abcdef". For SunOs 4.1.2
 * #define PTYRANGE0 "pqrstuvwxyzPQRST"
 * is recommended by Dan Jacobson.
 */
#undef PTYRANGE0
#undef PTYRANGE1

