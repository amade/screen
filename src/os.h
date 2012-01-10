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

#if !defined(HAVE_LONG_FILE_NAMES) && !defined(NAME_MAX)
#define NAME_MAX 14
#endif

#ifdef ISC
# ifdef ENAMETOOLONG
#  undef ENAMETOOLONG
# endif
# ifdef ENOTEMPTY
#  undef ENOTEMPTY
# endif
# include <sys/bsdtypes.h>
# include <net/errno.h>
#endif

#include <unistd.h>
#include <stdlib.h>

#ifndef HAVE_STRERROR
/* No macros, please */
#undef strerror
#endif

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif

#include <stdarg.h>

#include <time.h>
#include <sys/time.h>

#ifndef HAVE_GETCWD
# define getcwd(b,l) getwd(b)
#endif

#if defined(HAVE_SETRESUID) && !defined(HAVE_SETREUID)
# define setreuid(ruid, euid) setresuid(ruid, euid, -1)
# define setregid(rgid, egid) setresgid(rgid, egid, -1)
#endif

#if defined(HAVE_SETEUID) || defined(HAVE_SETREUID) || defined(HAVE_SETRESUID)
# define USE_SETEUID
#endif

#if !defined(HAVE__EXIT) && !defined(_exit)
#define _exit(x) exit(x)
#endif

#ifndef HAVE_UTIMES
# define utimes utime
#endif
#ifndef HAVE_VSNPRINTF
# define vsnprintf xvsnprintf
#endif

#ifdef BUILTIN_TELNET
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#if defined(USE_LOCALE) && (!defined(HAVE_SETLOCALE) || !defined(HAVE_STRFTIME))
# undef USE_LOCALE
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


/*****************************************************************
 *    file stuff
 */

#ifndef F_OK
#define F_OK 0
#endif
#ifndef X_OK
#define X_OK 1
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif

#ifndef S_IFIFO
#define S_IFIFO  0010000
#endif
#ifndef S_IREAD
#define S_IREAD  0000400
#endif
#ifndef S_IWRITE
#define S_IWRITE 0000200
#endif
#ifndef S_IEXEC
#define S_IEXEC  0000100
#endif

#if defined(S_IFIFO) && defined(S_IFMT) && !defined(S_ISFIFO)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#endif
#if defined(S_IFSOCK) && defined(S_IFMT) && !defined(S_ISSOCK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)
#endif
#if defined(S_IFCHR) && defined(S_IFMT) && !defined(S_ISCHR)
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#endif
#if defined(S_IFDIR) && defined(S_IFMT) && !defined(S_ISDIR)
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#if defined(S_IFLNK) && defined(S_IFMT) && !defined(S_ISLNK)
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#endif

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

#ifndef POSIX
#undef mkfifo
#define mkfifo(n,m) mknod(n,S_IFIFO|(m),0)
#endif

#if !defined(HAVE_LSTAT) && !defined(lstat)
# define lstat stat
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
# ifndef BSDWAIT /* if wait is NOT a union: */
#  define WTERMSIG(status) (status & 0177)
# else
#  define WTERMSIG(status) status.w_T.w_Termsig 
# endif
#endif

#ifndef WSTOPSIG
# ifndef BSDWAIT /* if wait is NOT a union: */
#  define WSTOPSIG(status) ((status >> 8) & 0377)
# else
#  define WSTOPSIG(status) status.w_S.w_Stopsig 
# endif
#endif

/* NET-2 uses WCOREDUMP */
#if defined(WCOREDUMP) && !defined(WIFCORESIG)
# define WIFCORESIG(status) WCOREDUMP(status)
#endif

#ifndef WIFCORESIG
# ifndef BSDWAIT /* if wait is NOT a union: */
#  define WIFCORESIG(status) (status & 0200)
# else
#  define WIFCORESIG(status) status.w_T.w_Coredump
# endif
#endif

#ifndef WEXITSTATUS
# ifndef BSDWAIT /* if wait is NOT a union: */
#  define WEXITSTATUS(status) ((status >> 8) & 0377)
# else
#  define WEXITSTATUS(status) status.w_T.w_Retcode
# endif
#endif

/*****************************************************************
 *    user defineable stuff
 */

#ifndef TERMCAP_BUFSIZE
# define TERMCAP_BUFSIZE 2048
#endif

#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif

/* 
 * you may try to vary this value. Use low values if your (VMS) system
 * tends to choke when pasting. Use high values if you want to test
 * how many characters your pty's can buffer.
 */
#define IOSIZE		4096

/* Changing those you won't be able to attach to your old sessions
 * when changing those values in official tree don't forget to bump
 * MSG_VERSION */
#define MAXTERMLEN	32
#define MAXLOGINLEN	256

