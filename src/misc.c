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
 */

#include <sys/types.h>
#include <sys/stat.h>		/* mkdir() declaration */
#include <signal.h>

#include "config.h"
#include "screen.h"
#include "extern.h"

#ifdef HAVE_FDWALK
static int close_func(void *, int);
#endif

char *SaveStr(register const char *str)
{
	register char *cp;

	if ((cp = malloc(strlen(str) + 1)) == NULL)
		Panic(0, "%s", strnomem);
	else
		strncpy(cp, str, strlen(str) + 1);
	return cp;
}

char *SaveStrn(register const char *str, int n)
{
	register char *cp;

	if ((cp = malloc(n + 1)) == NULL)
		Panic(0, "%s", strnomem);
	else {
		memmove(cp, (char *)str, n);
		cp[n] = 0;
	}
	return cp;
}

/* cheap strstr replacement */
char *InStr(char *str, const char *pat)
{
	int npat = strlen(pat);
	for (; *str; str++)
		if (!strncmp(str, pat, npat))
			return str;
	return 0;
}

void centerline(char *str, int y)
{
	int l, n;

	n = strlen(str);
	if (n > flayer->l_width - 1)
		n = flayer->l_width - 1;
	l = (flayer->l_width - 1 - n) / 2;
	LPutStr(flayer, str, n, &mchar_blank, l, y);
}

void leftline(char *str, int y, struct mchar *rend)
{
	int l, n;
	struct mchar mchar_dol;

	mchar_dol = mchar_blank;
	mchar_dol.image = '$';

	l = n = strlen(str);
	if (n > flayer->l_width - 1)
		n = flayer->l_width - 1;
	LPutStr(flayer, str, n, rend ? rend : &mchar_blank, 0, y);
	if (n != l)
		LPutChar(flayer, &mchar_dol, n, y);
}

char *Filename(char *s)
{
	register char *p = s;

	if (p)
		while (*p)
			if (*p++ == '/')
				s = p;
	return s;
}

char *stripdev(char *nam)
{
	if (nam == NULL)
		return NULL;
	if (strncmp(nam, "/dev/", 5) == 0)
		return nam + 5;
	return nam;
}

/*
 *    Signal handling
 */

void (*xsignal(int sig, void (*func) (int))) (int) {
	struct sigaction osa, sa;
	sa.sa_handler = func;
	(void)sigemptyset(&sa.sa_mask);
#ifdef SA_RESTART
	sa.sa_flags = (sig == SIGCHLD ? SA_RESTART : 0);
#else
	sa.sa_flags = 0;
#endif
	if (sigaction(sig, &sa, &osa))
		return (void (*)(int))-1;
	return osa.sa_handler;
}

/*
 *    uid/gid handling
 */

void xseteuid(int euid)
{
	if (seteuid(euid) == 0)
		return;
	seteuid(0);
	if (seteuid(euid))
		Panic(errno, "seteuid");
}

void xsetegid(int egid)
{
	if (setegid(egid))
		Panic(errno, "setegid");
}

void Kill(int pid, int sig)
{
	if (pid < 2)
		return;
	(void)kill(pid, sig);
}

#ifdef HAVE_FDWALK
/*
 * Modern versions of Solaris include fdwalk(3c) which allows efficient
 * implementation of closing open descriptors; this is helpful because
 * the default file descriptor limit has risen to 65k.
 */
static int close_func(void *cb_data, int fd)
{
	int except = *(int *)cb_data;
	if (fd > 2 && fd != except)
		(void)close(fd);
	return (0);
}

void closeallfiles(int except)
{
	(void)fdwalk(close_func, &except);
}

#else				/* HAVE_FDWALK */

void closeallfiles(int except)
{
	int f;
	f = getdtablesize();
	while (--f > 2)
		if (f != except)
			close(f);
}

#endif				/* HAVE_FDWALK */

/*
 *  Security - switch to real uid
 */

static int UserSTAT;

int UserContext()
{
	xseteuid(real_uid);
	xsetegid(real_gid);
	return 1;
}

void UserReturn(int val)
{
	xseteuid(eff_uid);
	xsetegid(eff_gid);
	UserSTAT = val;
}

int UserStatus()
{
	return UserSTAT;
}

int AddXChar(char *buf, int ch)
{
	char *p = buf;

	if (ch < ' ' || ch == 0x7f) {
		*p++ = '^';
		*p++ = ch ^ 0x40;
	} else if (ch >= 0x80) {
		*p++ = '\\';
		*p++ = (ch >> 6 & 7) + '0';
		*p++ = (ch >> 3 & 7) + '0';
		*p++ = (ch >> 0 & 7) + '0';
	} else
		*p++ = ch;
	return p - buf;
}

int AddXChars(char *buf, int len, char *str)
{
	char *p;

	if (str == 0) {
		*buf = 0;
		return 0;
	}
	len -= 4;		/* longest sequence produced by AddXChar() */
	for (p = buf; p < buf + len && *str; str++) {
		if (*str == ' ')
			*p++ = *str;
		else
			p += AddXChar(p, *str);
	}
	*p = 0;
	return p - buf;
}

void sleep1000(int msec)
{
	struct timeval t;

	t.tv_sec = (long)(msec / 1000);
	t.tv_usec = (long)((msec % 1000) * 1000);
	select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &t);
}
