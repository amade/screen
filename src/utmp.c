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
#include <sys/stat.h>
#include <fcntl.h>

#include "config.h"
#include "screen.h"
#include "extern.h"

/*
 *  UTNOKEEP: A (ugly) hack for apollo that does two things:
 *    1) Always close and reopen the utmp file descriptor. (I don't know
 *       for what reason this is done...)
 *    2) Implement an unsorted utmp file much like GETUTENT.
 *  (split into UT_CLOSE and UT_UNSORTED)
 */

#ifdef UTNOKEEP
#define UT_CLOSE
#define UT_UNSORTED
#endif

#ifdef UT_CLOSE
#undef UT_CLOSE
#define UT_CLOSE endutent()
#else
#define UT_CLOSE
#endif

#ifdef UTMPOK

static slot_t TtyNameSlot(char *);
static void makeuser(struct utmp *, char *, char *, int);
static void makedead(struct utmp *);
static int pututslot(slot_t, struct utmp *, char *, struct win *);
static struct utmp *getutslot(slot_t);
#ifndef GETUTENT
static struct utmp *getutent(void);
static void endutent(void);
static int initutmp(void);
static void setutent(void);
#endif
#if defined(linux) && defined(GETUTENT)
static struct utmp *xpututline(struct utmp *utmp);
#define pututline xpututline
#endif

static int utmpok;
static char UtmpName[] = UTMPFILE;
#ifndef UTMP_HELPER
static int utmpfd = -1;
#endif

#if !defined(GETUTENT) && !defined(UT_UNSORTED)
#ifdef GETTTYENT
#include <ttyent.h>
#else
struct ttyent {
	char *ty_name;
};
static void setttyent(void);
static struct ttyent *getttyent(void);
#endif
#endif				/* !GETUTENT && !UT_UNSORTED */

#undef  D_loginhost
#define D_loginhost D_utmp_logintty.ut_host
#ifndef UTHOST
#undef  D_loginhost
#define D_loginhost ((char *)0)
#endif

#endif				/* UTMPOK */

/*
 * SlotToggle - modify the utmp slot of the fore window.
 *
 * how > 0	do try to set a utmp slot.
 * how = 0	try to withdraw a utmp slot.
 *
 * w_slot = -1  window not logged in.
 * w_slot = 0   window not logged in, but should be logged in.
 *              (unable to write utmp, or detached).
 */

#ifndef UTMPOK
void SlotToggle(int how)
{
#ifdef UTMPFILE
	Msg(0, "Unable to modify %s.\n", UTMPFILE);
#else
	Msg(0, "Unable to modify utmp-database.\n");
#endif
}
#endif

#ifdef UTMPOK

void SlotToggle(int how)
{
	if (fore->w_type != W_TYPE_PTY) {
		Msg(0, "Can only work with normal windows.\n");
		return;
	}
	if (how) {
		if ((fore->w_slot == (slot_t) - 1) || (fore->w_slot == (slot_t) 0)) {
#ifdef USRLIMIT
			if (CountUsers() >= USRLIMIT) {
				Msg(0, "User limit reached.");
				return;
			}
#endif
			if (SetUtmp(fore) == 0)
				Msg(0, "This window is now logged in.");
			else
				Msg(0, "This window should now be logged in.");
			WindowChanged(fore, 'f');
		} else
			Msg(0, "This window is already logged in.");
	} else {
		if (fore->w_slot == (slot_t) - 1)
			Msg(0, "This window is already logged out\n");
		else if (fore->w_slot == (slot_t) 0) {
			Msg(0, "This window is not logged in.");
			fore->w_slot = (slot_t) - 1;
		} else {
			RemoveUtmp(fore);
			if (fore->w_slot != (slot_t) - 1)
				Msg(0, "What? Cannot remove Utmp slot?");
			else
				Msg(0, "This window is no longer logged in.");
#ifdef CAREFULUTMP
			CarefulUtmp();
#endif
			WindowChanged(fore, 'f');
		}
	}
}

#ifdef CAREFULUTMP

/* CAREFULUTMP: goodie for paranoid sysadmins: always leave one
 * window logged in
 */
void CarefulUtmp()
{
	struct win *p;

	if (!windows)		/* hopeless */
		return;
	for (p = windows; p; p = p->w_next)
		if (p->w_ptyfd >= 0 && p->w_slot != (slot_t) - 1)
			return;	/* found one, nothing to do */

	for (p = windows; p; p = p->w_next)
		if (p->w_ptyfd >= 0)	/* no zombies please */
			break;
	if (!p)
		return;		/* really hopeless */
	SetUtmp(p);
	Msg(0, "Window %d is now logged in.\n", p->w_number);
}
#endif				/* CAREFULUTMP */

void InitUtmp()
{
#ifndef UTMP_HELPER
	if ((utmpfd = open(UtmpName, O_RDWR)) == -1) {
		if (errno != EACCES)
			Msg(errno, "%s", UtmpName);
		utmpok = 0;
		return;
	}
#ifdef GETUTENT
	close(utmpfd);		/* it was just a test */
	utmpfd = -1;
#endif				/* GETUTENT */
#endif				/* UTMP_HELPER */
	utmpok = 1;
}

#ifdef USRLIMIT
int CountUsers()
{
	struct utmp *ut;
	int UserCount;

	if (!utmpok)
		return 0;
	UserCount = 0;
	setutent();
	while (ut = getutent())
		if (SLOT_USED(ut))
			UserCount++;
	UT_CLOSE;
	return UserCount;
}
#endif				/* USRLIMIT */

/*
 * the utmp entry for tty is located and removed.
 * it is stored in D_utmp_logintty.
 */
void RemoveLoginSlot()
{
	struct utmp u, *uu;

	D_loginslot = TtyNameSlot(D_usertty);
	if (D_loginslot == (slot_t) 0 || D_loginslot == (slot_t) - 1)
		return;
#ifdef UTMP_HELPER
	if (eff_uid)		/* helpers can't do login slots. sigh. */
#else
	if (!utmpok)
#endif
	{
		D_loginslot = 0;
	} else {
		if ((uu = getutslot(D_loginslot)) == 0) {
			D_loginslot = 0;
		} else {
			D_utmp_logintty = *uu;
			u = *uu;
			makedead(&u);
			if (pututslot(D_loginslot, &u, (char *)0, (struct win *)0) == 0)
				D_loginslot = 0;
		}
		UT_CLOSE;
	}
	if (D_loginslot == (slot_t) 0) {
		/* couldn't remove slot, do a 'mesg n' at least. */
		struct stat stb;
		char *tty;
		D_loginttymode = 0;
		if ((tty = ttyname(D_userfd)) && stat(tty, &stb) == 0 && stb.st_uid == real_uid && !CheckTtyname(tty)
		    && ((int)stb.st_mode & 0777) != 0666) {
			D_loginttymode = (int)stb.st_mode & 0777;
			chmod(D_usertty, stb.st_mode & 0600);
		}
	}
}

/*
 * D_utmp_logintty is reinserted into utmp
 */
void RestoreLoginSlot()
{
	char *tty;

	if (utmpok && D_loginslot != (slot_t) 0 && D_loginslot != (slot_t) - 1) {
		if (pututslot(D_loginslot, &D_utmp_logintty, D_loginhost, (struct win *)0) == 0)
			Msg(errno, "Could not write %s", UtmpName);
	}
	UT_CLOSE;
	D_loginslot = (slot_t) 0;
	if (D_loginttymode && (tty = ttyname(D_userfd)) && !CheckTtyname(tty))
		fchmod(D_userfd, D_loginttymode);
}

/*
 * Construct a utmp entry for window wi.
 * the hostname field reflects what we know about the user (display)
 * location. If d_loginhost is not set, then he is local and we write
 * down the name of his terminal line; else he is remote and we keep
 * the hostname here. The letter S and the window id will be appended.
 * A saved utmp entry in wi->w_savut serves as a template, usually.
 */

int SetUtmp(struct win *win)
{
	register slot_t slot;
	struct utmp u;
	int saved_ut;
#ifdef UTHOST
	char *p;
	char host[sizeof(D_loginhost) + 15];
#else
	char *host = 0;
#endif				/* UTHOST */

	win->w_slot = (slot_t) 0;
	if (!utmpok || win->w_type != W_TYPE_PTY)
		return -1;
	if ((slot = TtyNameSlot(win->w_tty)) == (slot_t) 0) {
		return -1;
	}

	memset((char *)&u, 0, sizeof(u));
	if ((saved_ut = memcmp((char *)&win->w_savut, (char *)&u, sizeof(u))))
		/* restore original, of which we will adopt all fields but ut_host */
		memmove((char *)&u, (char *)&win->w_savut, sizeof(u));

	if (!saved_ut)
		makeuser(&u, stripdev(win->w_tty), LoginName, win->w_pid);

#ifdef UTHOST
	host[sizeof(host) - 15] = '\0';
	if (display) {
		strncpy(host, D_loginhost, sizeof(host) - 15);
		if (D_loginslot != (slot_t) 0 && D_loginslot != (slot_t) - 1 && host[0] != '\0') {
			/*
			 * we want to set our ut_host field to something like
			 * ":ttyhf:s.0" or
			 * "faui45:s.0" or
			 * "132.199.81.4:s.0" (even this may hurt..), but not
			 * "faui45.informati"......:s.0
			 * HPUX uses host:0.0, so chop at "." and ":" (Eric Backus)
			 */
			for (p = host; *p; p++)
				if ((*p < '0' || *p > '9') && (*p != '.'))
					break;
			if (*p) {
				for (p = host; *p; p++)
					if (*p == '.' || (*p == ':' && p != host)) {
						*p = '\0';
						break;
					}
			}
		} else {
			strncpy(host + 1, stripdev(D_usertty), sizeof(host) - 15 - 1);
			host[0] = ':';
		}
	} else
		strncpy(host, "local", sizeof(host) - 15);

	sprintf(host + strlen(host), ":S.%d", win->w_number);

	strncpy(u.ut_host, host, sizeof(u.ut_host));
#endif				/* UTHOST */

	if (pututslot(slot, &u, host, win) == 0) {
		Msg(errno, "Could not write %s", UtmpName);
		UT_CLOSE;
		return -1;
	}
	win->w_slot = slot;
	memmove((char *)&win->w_savut, (char *)&u, sizeof(u));
	UT_CLOSE;
	return 0;
}

/*
 * if slot could be removed or was 0,  wi->w_slot = -1;
 * else not changed.
 */

int RemoveUtmp(struct win *win)
{
	struct utmp u, *uu;
	slot_t slot;

	slot = win->w_slot;
	if (!utmpok)
		return -1;
	if (slot == (slot_t) 0 || slot == (slot_t) - 1) {
		win->w_slot = (slot_t) - 1;
		return 0;
	}
	memset((char *)&u, 0, sizeof(u));
	if ((uu = getutslot(slot)) == 0) {
		Msg(0, "Utmp slot not found -> not removed");
		return -1;
	}
	memmove((char *)&win->w_savut, (char *)uu, sizeof(win->w_savut));
	u = *uu;
	makedead(&u);
	if (pututslot(slot, &u, (char *)0, win) == 0) {
		Msg(errno, "Could not write %s", UtmpName);
		UT_CLOSE;
		return -1;
	}
	win->w_slot = (slot_t) - 1;
	UT_CLOSE;
	return 0;
}

/*********************************************************************
 *
 *  routines using the getut* api
 */

#ifdef GETUTENT

#define SLOT_USED(u) (u->ut_type == USER_PROCESS)

static struct utmp *getutslot(slot_t slot)
{
	struct utmp u;
	memset((char *)&u, 0, sizeof(u));
	strncpy(u.ut_line, slot, sizeof(u.ut_line));
	setutent();
	return getutline(&u);
}

static int pututslot(slot_t slot, struct utmp *u, char *host, struct win *win)
{
	setutent();
	return pututline(u) != 0;
}

static void makedead(struct utmp *u)
{
	u->ut_type = DEAD_PROCESS;
	u->ut_exit.e_termination = 0;
	u->ut_exit.e_exit = 0;
	u->ut_user[0] = 0;	/* for Digital UNIX, kilbi@rad.rwth-aachen.de */
}

static void makeuser(struct utmp *u, char *line, char *user, int pid)
{
	time_t now;
	u->ut_type = USER_PROCESS;
	strncpy(u->ut_user, user, sizeof(u->ut_user));
	/* Now the tricky part... guess ut_id */
	strncpy(u->ut_id, line + 3, sizeof(u->ut_id));
	strncpy(u->ut_line, line, sizeof(u->ut_line));
	u->ut_pid = pid;
	/* must use temp variable because of NetBSD/sparc64, where
	 * ut_xtime is long(64) but time_t is int(32) */
	(void)time(&now);
	u->ut_time = now;
}

static slot_t TtyNameSlot(char *nam)
{
	return stripdev(nam);
}

#else				/* GETUTENT */

/*********************************************************************
 *
 *  getut emulation for systems lacking the api
 */

static struct utmp uent;

#define SLOT_USED(u) (u.ut_name[0] != 0)

static int initutmp()
{
	if (utmpfd >= 0)
		return 1;
	return (utmpfd = open(UtmpName, O_RDWR)) >= 0;
}

static void setutent()
{
	if (utmpfd >= 0)
		(void)lseek(utmpfd, (off_t) 0, 0);
}

static void endutent()
{
	if (utmpfd >= 0)
		close(utmpfd);
	utmpfd = -1;
}

static struct utmp *getutent()
{
	if (utmpfd < 0 && !initutmp())
		return 0;
	if (read(utmpfd, &uent, sizeof(uent)) != sizeof(uent))
		return 0;
	return &uent;
}

static struct utmp *getutslot(slot_t slot)
{
	if (utmpfd < 0 && !initutmp())
		return 0;
	lseek(utmpfd, (off_t) (slot * sizeof(struct utmp)), 0);
	if (read(utmpfd, &uent, sizeof(uent)) != sizeof(uent))
		return 0;
	return &uent;
}

static int pututslot(slot_t slot, struct utmp *u, char *host, struct win *win)
{
	if (utmpfd < 0 && !initutmp())
		return 0;
	lseek(utmpfd, (off_t) (slot * sizeof(*u)), 0);
	if (write(utmpfd, u, sizeof(*u)) != sizeof(*u))
		return 0;
	return 1;
}

static void makedead(struct utmp *u)
{
#ifdef UT_UNSORTED
	memset(u->ut_name, 0, sizeof(u->ut_name));
#ifdef UTHOST
	memset(u->ut_host, 0, sizeof(u->ut_host));
#endif
#else
	memset((char *)u, 0, sizeof(*u));
#endif
}

static void makeuser(struct utmp *u, char *line, char *user, int pid)
{
	time_t now;
	strncpy(u->ut_line, line, sizeof(u->ut_line));
	strncpy(u->ut_name, user, sizeof(u->ut_name));
	(void)time(&now);
	u->ut_time = now;
}

static slot_t TtyNameSlot(char *nam)
{
	slot_t slot;
	char *line;
#ifndef UT_UNSORTED
	struct ttyent *tp;
#endif

	line = stripdev(nam);
#ifdef UT_UNSORTED
	setutent();
	if (utmpfd < 0)
		return -1;
	for (slot = 0; getutent(); slot++)
		if (strcmp(uent.ut_line, line) == 0)
			break;
	UT_CLOSE;
#else
	slot = 1;
	setttyent();
	while ((tp = getttyent()) != 0 && strcmp(line, tp->ty_name) != 0)
		slot++;
#endif
	return slot;
}

#endif				/* GETUTENT */

/*********************************************************************
 *
 *  Cheap plastic imitation of ttyent routines.
 */

#if !defined(GETTTYENT) && !defined(GETUTENT) && !defined(UT_UNSORTED)

static char *tt, *ttnext;
static char ttys[] = "/etc/ttys";

static void setttyent()
{
	if (ttnext == 0) {
		struct stat s;
		register int f;
		register char *p, *ep;

		if ((f = open(ttys, O_RDONLY)) == -1 || fstat(f, &s) == -1)
			Panic(errno, ttys);
		if ((tt = malloc((unsigned)s.st_size + 1)) == 0)
			Panic(0, strnomem);
		if (read(f, tt, s.st_size) != s.st_size)
			Panic(errno, ttys);
		close(f);
		for (p = tt, ep = p + s.st_size; p < ep; p++)
			if (*p == '\n')
				*p = '\0';
		*p = '\0';
	}
	ttnext = tt;
}

static struct ttyent *getttyent()
{
	static struct ttyent t;

	if (*ttnext == '\0')
		return NULL;
	t.ty_name = ttnext + 2;
	ttnext += strlen(ttnext) + 1;
	return &t;
}

#endif				/* !GETTTYENT && !GETUTENT && !UT_UNSORTED */

#endif				/* UTMPOK */

/*********************************************************************
 *
 *  getlogin() replacement (for SVR4 machines)
 */

#if defined(BUGGYGETLOGIN) && defined(UTMP_FILE)
char *getlogin()
{
	char *tty = NULL;
#ifdef utmp
#undef utmp
#endif
	struct utmp u;
	static char retbuf[sizeof(u.ut_user) + 1];
	int fd;

	for (fd = 0; fd <= 2 && (tty = ttyname(fd)) == NULL; fd++) ;
	if ((tty == NULL) || CheckTtyname(tty) || ((fd = open(UTMP_FILE, O_RDONLY)) < 0))
		return NULL;
	tty = stripdev(tty);
	retbuf[0] = '\0';
	while (read(fd, (char *)&u, sizeof(struct utmp)) == sizeof(struct utmp)) {
		if (!strncmp(tty, u.ut_line, sizeof(u.ut_line))) {
			strncpy(retbuf, u.ut_user, sizeof(u.ut_user));
			if (u.ut_type == USER_PROCESS)
				break;
		}
	}
	close(fd);

	return *retbuf ? retbuf : NULL;
}
#endif				/* BUGGYGETLOGIN */

#if defined(linux) && defined(GETUTENT)
#undef pututline

/* aargh, linux' pututline returns void! */
struct utmp *xpututline(struct utmp *u)
{
	struct utmp *u2;
	pututline(u);
	setutent();
	u2 = getutline(u);
	if (u2 == 0)
		return u->ut_type == DEAD_PROCESS ? u : 0;
	return u->ut_type == u2->ut_type ? u : 0;
}
#endif
