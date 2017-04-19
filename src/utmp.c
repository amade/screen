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

#include "config.h"

#include "utmp.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>

#include "screen.h"

#ifdef HAVE_UTEMPTER
#include <utempter.h>
#endif

#include "misc.h"
#include "tty.h"
#include "winmsg.h"

/*
 *  we have a suid-root helper app that changes the utmp for us
 *  (won't work for login-slots)
 */
#if defined(HAVE_UTEMPTER)
#define UTMP_HELPER
#endif

#ifdef ENABLE_UTMP

static slot_t TtyNameSlot(char *);
static void makeuser(struct utmpx *, char *, char *, pid_t);
static void makedead(struct utmpx *);
static int pututslot(slot_t, struct utmpx *, char *, Window *);
static struct utmpx *getutslot(slot_t);

static int utmpok;
static char UtmpName[] = UTMPXFILE;
#ifndef UTMP_HELPER
static int utmpfd = -1;
#endif

#undef  D_loginhost
#define D_loginhost D_utmp_logintty.ut_host
#if !defined(HAVE_UT_HOST)
#undef  D_loginhost
#define D_loginhost ((char *)0)
#endif

#endif				/* ENABLE_UTMP */

/*
 * SlotToggle - modify the utmp slot of the fore window.
 *
 * how == true  try to set a utmp slot.
 * how == false try to withdraw a utmp slot.
 *
 * w_slot = -1  window not logged in.
 * w_slot = 0   window not logged in, but should be logged in.
 *              (unable to write utmp, or detached).
 */

#ifndef ENABLE_UTMP
void SlotToggle(bool how)
{
	(void)how; /* unused */
#ifdef UTMPXFILE
	Msg(0, "Unable to modify %s.\n", UTMPXFILE);
#else
	Msg(0, "Unable to modify utmp-database.\n");
#endif
}
#endif

#ifdef ENABLE_UTMP

void SlotToggle(bool how)
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
			WindowChanged(fore, WINESC_WFLAGS);
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
			WindowChanged(fore, WINESC_WFLAGS);
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
		if (p->w_ptyfd >= 0 && p->w_slot != (slot_t)-1)
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
		if ((tty = GetPtsPathOrSymlink(D_userfd)) && stat(tty, &stb) == 0 && stb.st_uid == real_uid && !CheckTtyname(tty)
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
	if (D_loginttymode && (tty = GetPtsPathOrSymlink(D_userfd)) && !CheckTtyname(tty))
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
#if defined(HAVE_UT_HOST)
	char *p;
	char host[ARRAY_SIZE(D_loginhost) + 15];
#else
	char *host = 0;
#endif/* HAVE_UT_HOST */

	win->w_slot = (slot_t) 0;
	if (!utmpok || win->w_type != W_TYPE_PTY)
		return -1;
	if ((slot = TtyNameSlot(win->w_tty)) == (slot_t) 0) {
		return -1;
	}

	memset((char *)&u, 0, sizeof(struct utmpx));
	if ((saved_ut = memcmp((char *)&win->w_savut, (char *)&u, sizeof(struct utmpx))))
		/* restore original, of which we will adopt all fields but ut_host */
		memmove((char *)&u, (char *)&win->w_savut, sizeof(struct utmpx));

	if (!saved_ut)
		makeuser(&u, stripdev(win->w_tty), LoginName, win->w_pid);

#if defined(HAVE_UT_HOST)
	host[ARRAY_SIZE(host) - 15] = '\0';
	if (display) {
		strncpy(host, D_loginhost, ARRAY_SIZE(host) - 15);
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
			strncpy(host + 1, stripdev(D_usertty), ARRAY_SIZE(host) - 15 - 1);
			host[0] = ':';
		}
	} else
		strncpy(host, "local", ARRAY_SIZE(host) - 15);

	sprintf(host + strlen(host), ":S.%d", win->w_number);

	strncpy(u.ut_host, host, ARRAY_SIZE(u.ut_host));
#endif				/* UTHOST */

	if (pututslot(slot, &u, host, win) == 0) {
		Msg(errno, "Could not write %s", UtmpName);
		return -1;
	}
	win->w_slot = slot;
	memmove((char *)&win->w_savut, (char *)&u, sizeof(struct utmpx));
	return 0;
}

/*
 * if slot could be removed or was 0,  win->w_slot = -1;
 * else not changed.
 */

int RemoveUtmp(Window *win)
{
	struct utmpx u, *uu;
	slot_t slot;

	slot = win->w_slot;
	if (!utmpok)
		return -1;
	if (slot == (slot_t)0 || slot == (slot_t)-1) {
		win->w_slot = (slot_t)-1;
		return 0;
	}
	memset((char *)&u, 0, sizeof(struct utmpx));
	if ((uu = getutslot(slot)) == 0) {
		Msg(0, "Utmp slot not found -> not removed");
		return -1;
	}
	memmove((char *)&win->w_savut, (char *)uu, sizeof(struct utmpx));
	u = *uu;
	makedead(&u);
	if (pututslot(slot, &u, (char *)0, win) == 0) {
		Msg(errno, "Could not write %s", UtmpName);
		return -1;
	}
	win->w_slot = (slot_t)-1;
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
	memset((char *)&u, 0, sizeof(struct utmpx));
	strncpy(u.ut_line, (char *)slot, ARRAY_SIZE(u.ut_line));
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
#else
	(void)host; /* unused */
	(void)win; /* unused */
#endif

	setutxent();
	return pututxline(u) != 0;
}

static void makedead(struct utmpx *u)
{
	u->ut_type = DEAD_PROCESS;
#if defined(HAVE_UT_EXIT)
	u->ut_exit.e_termination = 0;
	u->ut_exit.e_exit = 0;
#endif
	u->ut_user[0] = 0;	/* for Digital UNIX, kilbi@rad.rwth-aachen.de */
}

static void makeuser(struct utmpx *u, char *line, char *user, pid_t pid)
{
	time_t now;
	u->ut_type = USER_PROCESS;
	strncpy(u->ut_user, user, ARRAY_SIZE(u->ut_user));
	/* Now the tricky part... guess ut_id */
	strncpy(u->ut_id, line + 3, ARRAY_SIZE(u->ut_id));
	strncpy(u->ut_line, line, ARRAY_SIZE(u->ut_line));
	u->ut_pid = pid;
	/* must use temp variable because of NetBSD/sparc64, where
	 * ut_xtime is long(64) but time_t is int(32) */
	(void)time(&now);
	u->ut_tv.tv_sec = now;
}

static slot_t TtyNameSlot(char *name)
{
	return stripdev(name);
}

#endif				/* ENABLE_UTMP */

