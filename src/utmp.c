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
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "screen.h"

/* needs config.h */
#ifdef HAVE_UTEMPTER
#include <utempter.h>
#endif

#include "misc.h"
#include "tty.h"
#include "utmp.h"

/*
 *  we have a suid-root helper app that changes the utmp for us
 *  (won't work for login-slots)
 */
#if defined(HAVE_UTEMPTER)
#define UTMP_HELPER
#endif

#ifdef UTMPOK

static slot_t TtyNameSlot(char *);
static void makeuser(struct utmpx *, char *, char *, int);
static void makedead(struct utmpx *);
static int pututslot(slot_t, struct utmpx *, char *, Window *);
static struct utmpx *getutslot(slot_t);

static int utmpok;
static char UtmpName[] = UTMPFILE;
#ifndef UTMP_HELPER
static int utmpfd = -1;
#endif

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
	Window *p;

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
	close(utmpfd);		/* it was just a test */
	utmpfd = -1;
#endif				/* UTMP_HELPER */
	utmpok = 1;
}

/*
 * the utmp entry for tty is located and removed.
 * it is stored in D_utmp_logintty.
 */
void RemoveLoginSlot()
{
	struct utmpx u, *uu;

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
			if (pututslot(D_loginslot, &u, (char *)0, (Window *)0) == 0)
				D_loginslot = 0;
		}
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
		if (pututslot(D_loginslot, &D_utmp_logintty, D_loginhost, (Window *)0) == 0)
			Msg(errno, "Could not write %s", UtmpName);
	}
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

int SetUtmp(Window *win)
{
	slot_t slot;
	struct utmpx u;
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
		return -1;
	}
	win->w_slot = slot;
	memmove((char *)&win->w_savut, (char *)&u, sizeof(u));
	return 0;
}

/*
 * if slot could be removed or was 0,  wi->w_slot = -1;
 * else not changed.
 */

int RemoveUtmp(Window *win)
{
	struct utmpx u, *uu;
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
		return -1;
	}
	win->w_slot = (slot_t) - 1;
	return 0;
}

/*********************************************************************
 *
 *  routines using the getut* api
 */

#define SLOT_USED(u) (u->ut_type == USER_PROCESS)

static struct utmpx *getutslot(slot_t slot)
{
	struct utmpx u;
	memset((char *)&u, 0, sizeof(u));
	strncpy(u.ut_line, slot, sizeof(u.ut_line));
	setutxent();
	return getutxline(&u);
}

static int pututslot(slot_t slot, struct utmpx *u, char *host, Window *win)
{
	(void)slot; /* unused */
#ifdef HAVE_UTEMPTER
	if (eff_uid && win && win->w_ptyfd != -1) {
		/* sigh, linux hackers made the helper functions void */
		if (SLOT_USED(u))
			utempter_add_record(win->w_ptyfd, host);
		else
			utempter_remove_record(win->w_ptyfd);
		return 1;	/* pray for success */
	}
#endif

	setutxent();
	return pututxline(u) != 0;
}

static void makedead(struct utmpx *u)
{
	u->ut_type = DEAD_PROCESS;
	u->ut_exit.e_termination = 0;
	u->ut_exit.e_exit = 0;
	u->ut_user[0] = 0;	/* for Digital UNIX, kilbi@rad.rwth-aachen.de */
}

static void makeuser(struct utmpx *u, char *line, char *user, int pid)
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
	u->ut_tv.tv_sec = now;
}

static slot_t TtyNameSlot(char *nam)
{
	return stripdev(nam);
}

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
	struct utmpx u;
	static char retbuf[sizeof(u.ut_user) + 1];
	int fd;

	for (fd = 0; fd <= 2 && (tty = ttyname(fd)) == NULL; fd++) ;
	if ((tty == NULL) || CheckTtyname(tty) || ((fd = open(UTMP_FILE, O_RDONLY)) < 0))
		return NULL;
	tty = stripdev(tty);
	retbuf[0] = '\0';
	while (read(fd, (char *)&u, sizeof(struct utmpx)) == sizeof(struct utmpx)) {
		if (!strncmp(tty, u.ut_line, sizeof(u.ut_line))) {
			strncpy(retbuf, u.ut_user, sizeof(u.ut_user));
			retbuf[sizeof(u.ut_user)] = '\0';
			if (u.ut_type == USER_PROCESS)
				break;
		}
	}
	close(fd);

	return *retbuf ? retbuf : NULL;
}
#endif				/* BUGGYGETLOGIN */

