/* Copyright (c) 2008, 2009
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 *      Micah Cowan (micah@cowan.name)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
 * Copyright (c) 1993-2002, 2003, 2005, 2006, 2007
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

#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <signal.h>

#if !defined(MAXTERMLEN)
# if !defined(HAVE_LONG_FILE_NAMES)
#  define MAXTERMLEN 14
# else
#  define MAXTERMLEN 32
# endif
#endif

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#if defined(HAVE_SETRESUID) && !defined(HAVE_SETREUID)
# define setreuid(ruid, euid) setresuid(ruid, euid, -1)
# define setregid(rgid, egid) setresgid(rgid, egid, -1)
#endif

#ifndef HAVE_UTIMES
# define utimes utime
#endif
#ifndef HAVE_VSNPRINTF
# define vsnprintf xvsnprintf
#endif

/*****************************************************************
 *    terminal handling
 */

#include <termios.h>
#ifdef NCCS
# define MAXCC NCCS
#else
# define MAXCC 256
#endif

#ifndef VDISABLE
# ifdef _POSIX_VDISABLE
#  define VDISABLE _POSIX_VDISABLE
# else
#  define VDISABLE 0377
# endif /* _POSIX_VDISABLE */
#endif /* !VDISABLE */

/*****************************************************************
 *   utmp handling
 */

#ifdef GETUTENT
  typedef char *slot_t;
#else
  typedef int slot_t;
#endif

#if defined(UTMPOK) || defined(BUGGYGETLOGIN)
# include <utmp.h>

# ifndef UTMPFILE
#  ifdef UTMP_FILE
#   define UTMPFILE	UTMP_FILE
#  else
#   ifdef _PATH_UTMP
#    define UTMPFILE	_PATH_UTMP
#   else
#    define UTMPFILE	"/etc/utmp"
#   endif /* _PATH_UTMP */
#  endif
# endif

#endif /* UTMPOK || BUGGYGETLOGIN */

#if !defined(UTMPOK) && defined(USRLIMIT)
# undef USRLIMIT
#endif

#ifdef LOGOUTOK
# ifndef LOGINDEFAULT
#  define LOGINDEFAULT 0
# endif
#else
# ifdef LOGINDEFAULT
#  undef LOGINDEFAULT
# endif
# define LOGINDEFAULT 1
#endif

#if defined(UT_NAMESIZE) && !defined(MAXLOGINLEN)
# define MAXLOGINLEN UT_NAMESIZE
#else
# define MAXLOGINLEN 32
#endif

/*****************************************************************
 *    file stuff
 */

/*
 * SunOS 4.1.3: `man 2V open' has only one line that mentions O_NOBLOCK:
 *
 *     O_NONBLOCK     Same as O_NDELAY above.
 *
 * on the very same SunOS 4.1.3, I traced the open system call and found
 * that an open("/dev/ttyy08", O_RDWR|O_NONBLOCK|O_NOCTTY) was blocked,
 * whereas open("/dev/ttyy08", O_RDWR|O_NDELAY  |O_NOCTTY) went through.
 *
 * For this simple reason I now favour O_NDELAY. jw. 4.5.95
 */
#if !defined(O_NONBLOCK) && defined(O_NDELAY)
# define O_NONBLOCK O_NDELAY
#endif

#if !defined(FNBLOCK) && defined(FNONBLOCK)
# define FNBLOCK FNONBLOCK
#endif
#if !defined(FNBLOCK) && defined(FNDELAY)
# define FNBLOCK FNDELAY
#endif
#if !defined(FNBLOCK) && defined(O_NONBLOCK)
# define FNBLOCK O_NONBLOCK
#endif

/*****************************************************************
 *    signal handling
 */

#ifndef SIGCHLD
#define SIGCHLD SIGCLD
#endif

#define signal xsignal

/* used in screen.c and attacher.c */
#ifndef NSIG		/* kbeal needs these w/o SYSV */
# define NSIG 32
#endif /* !NSIG */


/*****************************************************************
 *    Wait stuff
 */

# include <sys/wait.h>

#ifndef WTERMSIG
# define WTERMSIG(status) (status & 0177)
#endif

#ifndef WSTOPSIG
# define WSTOPSIG(status) ((status >> 8) & 0377)
#endif

/* NET-2 uses WCOREDUMP */
#if defined(WCOREDUMP) && !defined(WIFCORESIG)
# define WIFCORESIG(status) WCOREDUMP(status)
#endif

#ifndef WIFCORESIG
# define WIFCORESIG(status) (status & 0200)
#endif

#ifndef WEXITSTATUS
# define WEXITSTATUS(status) ((status >> 8) & 0377)
#endif

/*****************************************************************
 *    user defineable stuff
 */

#ifndef TERMCAP_BUFSIZE
# define TERMCAP_BUFSIZE 2048
#endif

/*
 * you may try to vary this value. Use low values if your (VMS) system
 * tends to choke when pasting. Use high values if you want to test
 * how many characters your pty's can buffer.
 */
#define IOSIZE		4096

