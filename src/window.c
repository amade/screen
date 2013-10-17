/* Copyright (c) 2010
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
 * Copyright (c) 2008, 2009
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
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "config.h"

#include "screen.h"
#include "extern.h"
#include "fileio.h"
#include "logfile.h"		/* logfopen() */
#include "mark.h"
#include "misc.h"
#include "process.h"
#include "pty.h"
#include "resize.h"
#include "termcap.h"
#include "tty.h"
#include "utmp.h"

static void WinProcess(char **, int *);
static void WinRedisplayLine(int, int, int, int);
static void WinClearLine(int, int, int, int);
static int WinResize(int, int);
static void WinRestore(void);
static int DoAutolf(char *, int *, int);
static void ZombieProcess(char **, int *);
static void win_readev_fn(struct event *, char *);
static void win_writeev_fn(struct event *, char *);
static void win_resurrect_zombie_fn (struct event *, char *);
static int muchpending(struct win *, struct event *);
static void paste_slowev_fn(struct event *, char *);
static void pseu_readev_fn(struct event *, char *);
static void pseu_writeev_fn(struct event *, char *);
static void win_silenceev_fn(struct event *, char *);
static void win_destroyev_fn(struct event *, char *);

static int OpenDevice(char **, int, int *, char **);
static int ForkWindow(struct win *, char **, char *);

struct win **wtab;		/* window table */

int VerboseCreate = 0;		/* XXX move this to user.h */

char DefaultShell[] = "/bin/sh";
#ifndef HAVE_EXECVPE
static char DefaultPath[] = ":/usr/ucb:/bin:/usr/bin";
#endif

/* keep this in sync with the structure definition in window.h */
struct NewWindow nwin_undef = {
	-1,			/* StartAt */
	(char *)0,		/* aka */
	(char **)0,		/* args */
	(char *)0,		/* dir */
	(char *)0,		/* term */
	-1,			/* aflag */
	-1,			/* flowflag */
	-1,			/* lflag */
	-1,			/* histheight */
	-1,			/* monitor */
	-1,			/* wlock */
	-1,			/* silence */
	-1,			/* wrap */
	-1,			/* logging */
	-1,			/* slowpaste */
	-1,			/* gr */
	-1,			/* c1 */
	-1,			/* bce */
	-1,			/* encoding */
	(char *)0,		/* hstatus */
	(char *)0,		/* charset */
	0			/* poll_zombie_timeout */
};

struct NewWindow nwin_default = {
	0,			/* StartAt */
	0,			/* aka */
	ShellArgs,		/* args */
	0,			/* dir */
	screenterm,		/* term */
	0,			/* aflag */
	1 * FLOW_NOW,		/* flowflag */
	LOGINDEFAULT,		/* lflag */
	DEFAULTHISTHEIGHT,	/* histheight */
	MON_OFF,		/* monitor */
	WLOCK_OFF,		/* wlock */
	0,			/* silence */
	1,			/* wrap */
	0,			/* logging */
	0,			/* slowpaste */
	0,			/* gr */
	1,			/* c1 */
	0,			/* bce */
	0,			/* encoding */
	(char *)0,		/* hstatus */
	(char *)0,		/* charset */
	0			/* poll_zombie_timeout */
};

struct NewWindow nwin_options;

static int const_IOSIZE = IOSIZE;
static int const_one = 1;

void nwin_compose(struct NewWindow *def, struct NewWindow *new, struct NewWindow *res)
{
#define COMPOSE(x) res->x = new->x != nwin_undef.x ? new->x : def->x
	COMPOSE(StartAt);
	COMPOSE(aka);
	COMPOSE(args);
	COMPOSE(dir);
	COMPOSE(term);
	COMPOSE(aflag);
	COMPOSE(flowflag);
	COMPOSE(lflag);
	COMPOSE(histheight);
	COMPOSE(monitor);
	COMPOSE(wlock);
	COMPOSE(silence);
	COMPOSE(wrap);
	COMPOSE(Lflag);
	COMPOSE(slow);
	COMPOSE(gr);
	COMPOSE(c1);
	COMPOSE(bce);
	COMPOSE(encoding);
	COMPOSE(hstatus);
	COMPOSE(charset);
	COMPOSE(poll_zombie_timeout);
#undef COMPOSE
}

/*****************************************************************
 *
 *  The window layer functions
 */

struct LayFuncs WinLf = {
	WinProcess,
	0,
	WinRedisplayLine,
	WinClearLine,
	WinResize,
	WinRestore,
	0
};

static int DoAutolf(char *buf, int *lenp, int fr)
{
	char *p;
	int len = *lenp;
	int trunc = 0;

	for (p = buf; len > 0; p++, len--) {
		if (*p != '\r')
			continue;
		if (fr-- <= 0) {
			trunc++;
			len--;
		}
		if (len == 0)
			break;
		memmove(p + 1, p, len++);
		p[1] = '\n';
	}
	*lenp = p - buf;
	return trunc;
}

static void WinProcess(char **bufpp, int *lenp)
{
	int l2 = 0, f, *ilen, l = *lenp, trunc;
	char *ibuf;

	fore = (struct win *)flayer->l_data;

	if (fore->w_type == W_TYPE_GROUP) {
		*bufpp += *lenp;
		*lenp = 0;
		return;
	}
	if (fore->w_ptyfd < 0) {	/* zombie? */
		ZombieProcess(bufpp, lenp);
		return;
	}

	if (W_UWP(fore)) {
		/* we send the user input to our pseudowin */
		ibuf = fore->w_pwin->p_inbuf;
		ilen = &fore->w_pwin->p_inlen;
		f = sizeof(fore->w_pwin->p_inbuf) - *ilen;
	} else {
		/* we send the user input to the window */
		ibuf = fore->w_inbuf;
		ilen = &fore->w_inlen;
		f = sizeof(fore->w_inbuf) - *ilen;
	}

	if (l > f)
		l = f;
	if (l > 0) {
		l2 = l;
		memmove(ibuf + *ilen, *bufpp, l2);
		if (fore->w_autolf && (trunc = DoAutolf(ibuf + *ilen, &l2, f - l2)))
			l -= trunc;
		*ilen += l2;
		*bufpp += l;
		*lenp -= l;
		return;
	}
}

static void ZombieProcess(char **bufpp, int *lenp)
{
	int l = *lenp;
	char *buf = *bufpp, b1[10], b2[10];

	fore = (struct win *)flayer->l_data;

	*bufpp += *lenp;
	*lenp = 0;
	for (; l-- > 0; buf++) {
		if (*(unsigned char *)buf == ZombieKey_destroy) {
			KillWindow(fore);
			return;
		}
		if (*(unsigned char *)buf == ZombieKey_resurrect) {
			WriteString(fore, "\r\n", 2);
			RemakeWindow(fore);
			return;
		}
	}
	b1[AddXChar(b1, ZombieKey_destroy)] = '\0';
	b2[AddXChar(b2, ZombieKey_resurrect)] = '\0';
	Msg(0, "Press %s to destroy or %s to resurrect window", b1, b2);
}

static void WinRedisplayLine(int y, int from, int to, int isblank)
{
	if (y < 0)
		return;
	fore = (struct win *)flayer->l_data;
	if (from == 0 && y > 0 && fore->w_mlines[y - 1].image[fore->w_width] == 0)
		LCDisplayLineWrap(&fore->w_layer, &fore->w_mlines[y], y, from, to, isblank);
	else
		LCDisplayLine(&fore->w_layer, &fore->w_mlines[y], y, from, to, isblank);
}

static void WinClearLine(int y, int xs, int xe, int bce)
{
	fore = (struct win *)flayer->l_data;
	LClearLine(flayer, y, xs, xe, bce, &fore->w_mlines[y]);
}

static int WinResize(int wi, int he)
{
	fore = (struct win *)flayer->l_data;
	ChangeWindowSize(fore, wi, he, fore->w_histheight);
	return 0;
}

static void WinRestore()
{
	Canvas *cv;
	fore = (struct win *)flayer->l_data;
	for (cv = flayer->l_cvlist; cv; cv = cv->c_next) {
		display = cv->c_display;
		if (cv != D_forecv)
			continue;
		/* ChangeScrollRegion(fore->w_top, fore->w_bot); */
		KeypadMode(fore->w_keypad);
		CursorkeysMode(fore->w_cursorkeys);
		SetFlow(fore->w_flow & FLOW_NOW);
		InsertMode(fore->w_insert);
		ReverseVideo(fore->w_revvid);
		CursorVisibility(fore->w_curinv ? -1 : fore->w_curvvis);
		MouseMode(fore->w_mouse);
		BracketedPasteMode(fore->w_bracketed);
		CursorStyle(fore->w_cursorstyle);
	}
}

/*****************************************************************/

/*
 * DoStartLog constructs a path for the "want to be logfile" in buf and
 * attempts logfopen.
 *
 * returns 0 on success.
 */
int DoStartLog(struct win *w, char *buf, int bufsize)
{
	int n;
	if (!w || !buf)
		return -1;

	strncpy(buf, MakeWinMsg(screenlogfile, w, '%'), bufsize - 1);

	if (w->w_log != NULL)
		logfclose(w->w_log);

	if ((w->w_log = logfopen(buf, islogfile(buf) ? NULL : secfopen(buf, "a"))) == NULL)
		return -2;
	if (!logflushev.queued) {
		n = log_flush ? log_flush : (logtstamp_after + 4) / 5;
		if (n) {
			SetTimeout(&logflushev, n * 1000);
			evenq(&logflushev);
		}
	}
	return 0;
}

/*
 * Umask & wlock are set for the user of the display,
 * The display d (if specified) switches to that window.
 */
int MakeWindow(struct NewWindow *newwin)
{
	register struct win **pp, *p;
	register int n, i;
	int f = -1;
	struct NewWindow nwin;
	int type, startat;
	char *TtyName;

	if (!wtab) {
		if (!maxwin)
			maxwin = MAXWIN;
		wtab = calloc(maxwin, sizeof(struct win *));
	}

	nwin_compose(&nwin_default, newwin, &nwin);

	startat = nwin.StartAt < maxwin ? nwin.StartAt : 0;
	pp = wtab + startat;

	do {
		if (*pp == 0)
			break;
		if (++pp == wtab + maxwin)
			pp = wtab;
	}
	while (pp != wtab + startat);
	if (*pp) {
		Msg(0, "No more windows.");
		return -1;
	}
#if defined(USRLIMIT) && defined(UTMPOK)
	/*
	 * Count current number of users, if logging windows in.
	 */
	if (nwin.lflag && CountUsers() >= USRLIMIT) {
		Msg(0, "User limit reached.  Window will not be logged in.");
		nwin.lflag = 0;
	}
#endif
	n = pp - wtab;

	if ((f = OpenDevice(nwin.args, nwin.lflag, &type, &TtyName)) < 0)
		return -1;
	if (type == W_TYPE_GROUP)
		f = -1;

	if ((p = calloc(1, sizeof(struct win))) == 0) {
		close(f);
		Msg(0, "%s", strnomem);
		return -1;
	}
#ifdef UTMPOK
	if (type != W_TYPE_PTY)
		nwin.lflag = 0;
#endif

	p->w_type = type;

	/* save the command line so that zombies can be resurrected */
	for (i = 0; nwin.args[i] && i < MAXARGS - 1; i++)
		p->w_cmdargs[i] = SaveStr(nwin.args[i]);
	p->w_cmdargs[i] = 0;
	if (nwin.dir)
		p->w_dir = SaveStr(nwin.dir);
	if (nwin.term)
		p->w_term = SaveStr(nwin.term);

	p->w_number = n;
	p->w_group = 0;
	if (fore && fore->w_type == W_TYPE_GROUP)
		p->w_group = fore;
	else if (fore && fore->w_group)
		p->w_group = fore->w_group;
	p->w_layer.l_next = 0;
	p->w_layer.l_bottom = &p->w_layer;
	p->w_layer.l_layfn = &WinLf;
	p->w_layer.l_data = (char *)p;
	p->w_savelayer = &p->w_layer;
	p->w_pdisplay = 0;
	p->w_lastdisp = 0;

	p->w_ptyfd = f;
	p->w_aflag = nwin.aflag;
	p->w_flow = nwin.flowflag | ((nwin.flowflag & FLOW_AUTOFLAG) ? (FLOW_AUTO | FLOW_NOW) : FLOW_AUTO);
	if (!nwin.aka)
		nwin.aka = Filename(nwin.args[0]);
	strncpy(p->w_akabuf, nwin.aka, sizeof(p->w_akabuf));
	if ((nwin.aka = strrchr(p->w_akabuf, '|')) != NULL) {
		p->w_autoaka = 0;
		*nwin.aka++ = 0;
		p->w_title = nwin.aka;
		p->w_akachange = nwin.aka + strlen(nwin.aka);
	} else
		p->w_title = p->w_akachange = p->w_akabuf;
	if (nwin.hstatus)
		p->w_hstatus = SaveStr(nwin.hstatus);
	p->w_monitor = nwin.monitor;
	/*
	 * defsilence by Lloyd Zusman (zusman_lloyd@jpmorgan.com)
	 */
	p->w_silence = nwin.silence;
	p->w_silencewait = SilenceWait;
	p->w_slowpaste = nwin.slow;

	p->w_norefresh = 0;
	strncpy(p->w_tty, TtyName, MAXSTR);

	if (ChangeWindowSize(p, display ? D_forecv->c_xe - D_forecv->c_xs + 1 : 80,
			     display ? D_forecv->c_ye - D_forecv->c_ys + 1 : 24, nwin.histheight)) {
		FreeWindow(p);
		return -1;
	}

	p->w_encoding = nwin.encoding;
	ResetWindow(p);		/* sets w_wrap, w_c1, w_gr */

	if (nwin.charset)
		SetCharsets(p, nwin.charset);

	if (VerboseCreate && type != W_TYPE_GROUP) {
		struct display *d = display;	/* WriteString zaps display */

		WriteString(p, ":screen (", 9);
		WriteString(p, p->w_title, strlen(p->w_title));
		WriteString(p, "):", 2);
		for (f = 0; p->w_cmdargs[f]; f++) {
			WriteString(p, " ", 1);
			WriteString(p, p->w_cmdargs[f], strlen(p->w_cmdargs[f]));
		}
		WriteString(p, "\r\n", 2);
		display = d;
	}

	p->w_deadpid = 0;
	p->w_pid = 0;
	p->w_pwin = 0;

	if (type == W_TYPE_PTY) {
		p->w_pid = ForkWindow(p, nwin.args, TtyName);
		if (p->w_pid < 0) {
			FreeWindow(p);
			return -1;
		}
	}

	/*
	 * Place the new window at the head of the most-recently-used list.
	 */
	if (display && D_fore)
		D_other = D_fore;
	*pp = p;
	p->w_next = windows;
	windows = p;

	if (type == W_TYPE_GROUP) {
		SetForeWindow(p);
		Activate(p->w_norefresh);
		WindowChanged((struct win *)0, 'w');
		WindowChanged((struct win *)0, 'W');
		WindowChanged((struct win *)0, 0);
		return n;
	}

	p->w_lflag = nwin.lflag;
#ifdef UTMPOK
	p->w_slot = (slot_t) - 1;
#ifdef LOGOUTOK
	if (nwin.lflag & 1)
#endif				/* LOGOUTOK */
	{
		p->w_slot = (slot_t) 0;
		if (display || (p->w_lflag & 2))
			SetUtmp(p);
	}
#ifdef CAREFULUTMP
	CarefulUtmp();		/* If all 've been zombies, we've had no slot */
#endif
#endif				/* UTMPOK */

	if (nwin.Lflag) {
		char buf[1024];
		DoStartLog(p, buf, sizeof(buf));
	}

	/* Is this all where I have to init window poll timeout? */
	if (nwin.poll_zombie_timeout)
		p->w_poll_zombie_timeout = nwin.poll_zombie_timeout;

	p->w_zombieev.type = EV_TIMEOUT;
	p->w_zombieev.data = (char *)p;
	p->w_zombieev.handler = win_resurrect_zombie_fn;

	p->w_readev.fd = p->w_writeev.fd = p->w_ptyfd;
	p->w_readev.type = EV_READ;
	p->w_writeev.type = EV_WRITE;
	p->w_readev.data = p->w_writeev.data = (char *)p;
	p->w_readev.handler = win_readev_fn;
	p->w_writeev.handler = win_writeev_fn;
	p->w_writeev.condpos = &p->w_inlen;
	evenq(&p->w_readev);
	evenq(&p->w_writeev);
	p->w_paster.pa_slowev.type = EV_TIMEOUT;
	p->w_paster.pa_slowev.data = (char *)&p->w_paster;
	p->w_paster.pa_slowev.handler = paste_slowev_fn;
	p->w_silenceev.type = EV_TIMEOUT;
	p->w_silenceev.data = (char *)p;
	p->w_silenceev.handler = win_silenceev_fn;
	if (p->w_silence > 0) {
		SetTimeout(&p->w_silenceev, p->w_silencewait * 1000);
		evenq(&p->w_silenceev);
	}
	p->w_destroyev.type = EV_TIMEOUT;
	p->w_destroyev.data = 0;
	p->w_destroyev.handler = win_destroyev_fn;

	SetForeWindow(p);
	Activate(p->w_norefresh);
	WindowChanged((struct win *)0, 'w');
	WindowChanged((struct win *)0, 'W');
	WindowChanged((struct win *)0, 0);
	return n;
}

/*
 * Resurrect a window from Zombie state.
 * The command vector is therefore stored in the window structure.
 * Note: The terminaltype defaults to screenterm again, the current
 * working directory is lost.
 */
int RemakeWindow(struct win *p)
{
	char *TtyName;
	int lflag, f;

	lflag = nwin_default.lflag;
	if ((f = OpenDevice(p->w_cmdargs, lflag, &p->w_type, &TtyName)) < 0)
		return -1;

	evdeq(&p->w_destroyev);	/* no re-destroy of resurrected zombie */

	strncpy(p->w_tty, *TtyName ? TtyName : p->w_title, MAXSTR - 1);

	p->w_ptyfd = f;
	p->w_readev.fd = f;
	p->w_writeev.fd = f;
	evenq(&p->w_readev);
	evenq(&p->w_writeev);

	if (VerboseCreate) {
		struct display *d = display;	/* WriteString zaps display */

		WriteString(p, ":screen (", 9);
		WriteString(p, p->w_title, strlen(p->w_title));
		WriteString(p, "):", 2);
		for (f = 0; p->w_cmdargs[f]; f++) {
			WriteString(p, " ", 1);
			WriteString(p, p->w_cmdargs[f], strlen(p->w_cmdargs[f]));
		}
		WriteString(p, "\r\n", 2);
		display = d;
	}

	p->w_deadpid = 0;
	p->w_pid = 0;
	if (p->w_type == W_TYPE_PTY) {
		p->w_pid = ForkWindow(p, p->w_cmdargs, TtyName);
		if (p->w_pid < 0)
			return -1;
	}
#ifdef UTMPOK
	if (p->w_slot == (slot_t) 0 && (display || (p->w_lflag & 2)))
		SetUtmp(p);
#ifdef CAREFULUTMP
	CarefulUtmp();		/* If all 've been zombies, we've had no slot */
#endif
#endif
	WindowChanged(p, 'f');
	return p->w_number;
}

void CloseDevice(struct win *wp)
{
	if (wp->w_ptyfd < 0)
		return;
	if (wp->w_type == W_TYPE_PTY) {
		/* pty 4 SALE */
		(void)chmod(wp->w_tty, 0666);
		(void)chown(wp->w_tty, 0, 0);
	}
	close(wp->w_ptyfd);
	wp->w_ptyfd = -1;
	wp->w_tty[0] = 0;
	evdeq(&wp->w_readev);
	evdeq(&wp->w_writeev);
	wp->w_readev.fd = wp->w_writeev.fd = -1;
}

void FreeWindow(struct win *wp)
{
	struct display *d;
	int i;
	Canvas *cv, *ncv;
	struct layer *l;

	if (wp->w_pwin)
		FreePseudowin(wp);
#ifdef UTMPOK
	RemoveUtmp(wp);
#endif
	CloseDevice(wp);

	if (wp == console_window) {
		TtyGrabConsole(-1, -1, "free");
		console_window = 0;
	}
	if (wp->w_log != NULL)
		logfclose(wp->w_log);
	ChangeWindowSize(wp, 0, 0, 0);

	if (wp->w_type == W_TYPE_GROUP) {
		struct win *win;
		for (win = windows; win; win = win->w_next)
			if (win->w_group == wp)
				win->w_group = wp->w_group;
	}

	if (wp->w_hstatus)
		free(wp->w_hstatus);
	for (i = 0; wp->w_cmdargs[i]; i++)
		free(wp->w_cmdargs[i]);
	if (wp->w_dir)
		free(wp->w_dir);
	if (wp->w_term)
		free(wp->w_term);
	for (d = displays; d; d = d->d_next) {
		if (d->d_other == wp)
			d->d_other = d->d_fore && d->d_fore->w_next != wp ? d->d_fore->w_next : wp->w_next;
		if (d->d_fore == wp)
			d->d_fore = NULL;
		for (cv = d->d_cvlist; cv; cv = cv->c_next) {
			for (l = cv->c_layer; l; l = l->l_next)
				if (l->l_layfn == &WinLf)
					break;
			if (!l)
				continue;
			if ((struct win *)l->l_data != wp)
				continue;
			if (cv->c_layer == wp->w_savelayer)
				wp->w_savelayer = 0;
			KillLayerChain(cv->c_layer);
		}
	}
	if (wp->w_savelayer)
		KillLayerChain(wp->w_savelayer);
	for (cv = wp->w_layer.l_cvlist; cv; cv = ncv) {
		ncv = cv->c_lnext;
		cv->c_layer = &cv->c_blank;
		cv->c_blank.l_cvlist = cv;
		cv->c_lnext = 0;
		cv->c_xoff = cv->c_xs;
		cv->c_yoff = cv->c_ys;
		RethinkViewportOffsets(cv);
	}
	wp->w_layer.l_cvlist = 0;
	if (flayer == &wp->w_layer)
		flayer = 0;
	LayerCleanupMemory(&wp->w_layer);

	evdeq(&wp->w_readev);	/* just in case */
	evdeq(&wp->w_writeev);	/* just in case */
	evdeq(&wp->w_silenceev);
	evdeq(&wp->w_zombieev);
	evdeq(&wp->w_destroyev);
	FreePaster(&wp->w_paster);
	free((char *)wp);
}

static int OpenDevice(char **args, int lflag, int *typep, char **namep)
{
	char *arg = args[0];
	struct stat st;
	int f;

	if (!arg)
		return -1;
	if (strcmp(arg, "//group") == 0) {
		*typep = W_TYPE_GROUP;
		*namep = "telnet";
		return 0;
	}
	if (strncmp(arg, "//", 2) == 0) {
		Msg(0, "Invalid argument '%s'", arg);
		return -1;
	} else if ((stat(arg, &st)) == 0 && S_ISCHR(st.st_mode)) {
		if (access(arg, R_OK | W_OK) == -1) {
			Msg(errno, "Cannot access line '%s' for R/W", arg);
			return -1;
		}
		if ((f = OpenTTY(arg, args[1])) < 0)
			return -1;
		lflag = 0;
		*typep = W_TYPE_PLAIN;
		*namep = arg;
	} else {
		*typep = W_TYPE_PTY;
		f = OpenPTY(namep);
		if (f == -1) {
			Msg(0, "No more PTYs.");
			return -1;
		}
#ifdef TIOCPKT
		{
			int flag = 1;
			if (ioctl(f, TIOCPKT, (char *)&flag)) {
				Msg(errno, "TIOCPKT ioctl");
				close(f);
				return -1;
			}
		}
#endif				/* TIOCPKT */
	}
	(void)fcntl(f, F_SETFL, FNBLOCK);
	/*
	 * Tenebreux (zeus@ns.acadiacom.net) has Linux 1.3.70 where select
	 * gets confused in the following condition:
	 * Open a pty-master side, request a flush on it, then set packet
	 * mode and call select(). Select will return a possible read, where
	 * the one byte response to the flush can be found. Select will
	 * thereafter return a possible read, which yields I/O error.
	 *
	 * If we request another flush *after* switching into packet mode,
	 * this I/O error does not occur. We receive a single response byte
	 * although we send two flush requests now.
	 *
	 * Maybe we should not flush at all.
	 *
	 * 10.5.96 jw.
	 */
	if (*typep == W_TYPE_PTY || *typep == W_TYPE_PLAIN)
		tcflush(f, TCIOFLUSH);

	if (*typep != W_TYPE_PTY)
		return f;

#ifndef PTYROFS
#ifdef PTYGROUP
	if (chown(*namep, real_uid, PTYGROUP) && !eff_uid)
#else
	if (chown(*namep, real_uid, real_gid) && !eff_uid)
#endif
	{
		Msg(errno, "chown tty");
		close(f);
		return -1;
	}
#ifdef UTMPOK
	if (chmod(*namep, lflag ? TtyMode : (TtyMode & ~022)) && !eff_uid)
#else
	if (chmod(*namep, TtyMode) && !eff_uid)
#endif
	{
		Msg(errno, "chmod tty");
		close(f);
		return -1;
	}
#endif
	return f;
}

/*
 * Fields w_width, w_height, aflag, number (and w_tty)
 * are read from struct win *win. No fields written.
 * If pwin is nonzero, filedescriptors are distributed
 * between win->w_tty and open(ttyn)
 *
 */
static int ForkWindow(struct win *win, char **args, char *ttyn)
{
	int pid;
	char tebuf[25];
	char ebuf[20];
	char shellbuf[7 + MAXPATHLEN];
	char *proc;
#ifndef TIOCSWINSZ
	char libuf[20], cobuf[20];
#endif
	int newfd;
	int w = win->w_width;
	int h = win->w_height;
	int i, pat, wfdused;
	struct pseudowin *pwin = win->w_pwin;
	int slave = -1;
	struct sigaction sigact;

#ifdef O_NOCTTY
	if (pty_preopen) {
		if ((slave = open(ttyn, O_RDWR | O_NOCTTY)) == -1) {
			Msg(errno, "ttyn");
			return -1;
		}
	}
#endif
	proc = *args;
	if (proc == 0) {
		args = ShellArgs;
		proc = *args;
	}
	fflush(stdout);
	fflush(stderr);
	switch (pid = fork()) {
	case -1:
		Msg(errno, "fork");
		break;
	case 0:
		sigemptyset (&sigact.sa_mask);
		sigact.sa_flags = 0;
		sigact.sa_handler = SIG_DFL;
		sigaction(SIGHUP, &sigact, NULL);
		sigaction(SIGINT, &sigact, NULL);
		sigaction(SIGQUIT, &sigact, NULL);
		sigaction(SIGTERM, &sigact, NULL);
		sigaction(SIGTTIN, &sigact, NULL);
		sigaction(SIGTTOU, &sigact, NULL);

		displays = 0;	/* beware of Panic() */
		if (setgid(real_gid) || setuid(real_uid))
			Panic(errno, "Setuid/gid");
		eff_uid = real_uid;
		eff_gid = real_gid;
		if (!pwin)	/* ignore directory if pseudo */
			if (win->w_dir && *win->w_dir && chdir(win->w_dir))
				Panic(errno, "Cannot chdir to %s", win->w_dir);

		if (display) {
			brktty(D_userfd);
			freetty();
		} else
			brktty(-1);
		if (slave != -1) {
			close(0);
			dup(slave);
			close(slave);
			closeallfiles(win->w_ptyfd);
			slave = dup(0);
		} else
			closeallfiles(win->w_ptyfd);
		/* Close the three /dev/null descriptors */
		close(0);
		close(1);
		close(2);
		newfd = -1;
		/*
		 * distribute filedescriptors between the ttys
		 */
		pat = pwin ? pwin->p_fdpat : ((F_PFRONT << (F_PSHIFT * 2)) | (F_PFRONT << F_PSHIFT) | F_PFRONT);
		wfdused = 0;
		for (i = 0; i < 3; i++) {
			if (pat & F_PFRONT << F_PSHIFT * i) {
				if (newfd < 0) {
#ifdef O_NOCTTY
					if (separate_sids)
						newfd = open(ttyn, O_RDWR);
					else
						newfd = open(ttyn, O_RDWR | O_NOCTTY);
#else
					newfd = open(ttyn, O_RDWR);
#endif
					if (newfd < 0)
						Panic(errno, "Cannot open %s", ttyn);
				} else
					dup(newfd);
			} else {
				dup(win->w_ptyfd);
				wfdused = 1;
			}
		}
		if (wfdused) {
			/*
			 * the pseudo window process should not be surprised with a
			 * nonblocking filedescriptor. Poor Backend!
			 */
			if (fcntl(win->w_ptyfd, F_SETFL, 0))
				Msg(errno, "Warning: clear NBLOCK fcntl failed");
		}
		close(win->w_ptyfd);
		if (slave != -1)
			close(slave);
		if (newfd >= 0) {
			struct mode fakemode, *modep;
			if (fgtty(newfd))
				Msg(errno, "fgtty");
			if (display) {
				modep = &D_OldMode;
			} else {
				modep = &fakemode;
				InitTTY(modep, 0);
			}
			/* We only want echo if the users input goes to the pseudo
			 * and the pseudo's stdout is not send to the window.
			 */
			if (pwin && (!(pat & F_UWP) || (pat & F_PBACK << F_PSHIFT))) {
#if defined(POSIX) || defined(TERMIO)
				modep->tio.c_lflag &= ~ECHO;
				modep->tio.c_iflag &= ~ICRNL;
#else
				modep->m_ttyb.sg_flags &= ~ECHO;
#endif
			}
			SetTTY(newfd, modep);
#ifdef TIOCSWINSZ
			glwz.ws_col = w;
			glwz.ws_row = h;
			(void)ioctl(newfd, TIOCSWINSZ, (char *)&glwz);
#endif
			/* Always turn off nonblocking mode */
			(void)fcntl(newfd, F_SETFL, 0);
		}
#ifndef TIOCSWINSZ
		sprintf(libuf, "LINES=%d", h);
		sprintf(cobuf, "COLUMNS=%d", w);
		NewEnv[5] = libuf;
		NewEnv[6] = cobuf;
#endif
		NewEnv[2] = MakeTermcap(display == 0 || win->w_aflag);
		strncpy(shellbuf, "SHELL=", 7);
		strncpy(shellbuf + 6, ShellProg + (*ShellProg == '-'), sizeof(shellbuf) - 6);
		NewEnv[4] = shellbuf;
		if (win->w_term && *win->w_term && strcmp(screenterm, win->w_term) && (strlen(win->w_term) < 20)) {
			char *s1, *s2, tl;

			sprintf(tebuf, "TERM=%s", win->w_term);
			tl = strlen(win->w_term);
			NewEnv[1] = tebuf;
			if ((s1 = strchr(NewEnv[2], '|'))) {
				if ((s2 = strchr(++s1, '|'))) {
					if (strlen(NewEnv[2]) - (s2 - s1) + tl < 1024) {
						memmove(s1 + tl, s2, strlen(s2) + 1);
						memmove(s1, win->w_term, tl);
					}
				}
			}
		}
		sprintf(ebuf, "WINDOW=%d", win->w_number);
		NewEnv[3] = ebuf;

		if (*proc == '-')
			proc++;
		if (!*proc)
			proc = DefaultShell;
		execvpe(proc, args, NewEnv);
		Panic(errno, "Cannot exec '%s'", proc);
	default:
		break;
	}
	if (slave != -1)
		close(slave);
	return pid;
}

#ifndef HAVE_EXECVPE
void execvpe(char *prog, char **args, char **env)
{
	register char *path = NULL, *p;
	char buf[1024];
	char *shargs[MAXARGS + 1];
	register int i, eaccess = 0;

	if (strrchr(prog, '/'))
		path = "";
	if (!path && !(path = getenv("PATH")))
		path = DefaultPath;
	do {
		for (p = buf; *path && *path != ':'; path++)
			if (p - buf < (int)sizeof(buf) - 2)
				*p++ = *path;
		if (p > buf)
			*p++ = '/';
		if (p - buf + strlen(prog) >= sizeof(buf) - 1)
			continue;
		strcpy(p, prog);
		execve(buf, args, env);
		switch (errno) {
		case ENOEXEC:
			shargs[0] = DefaultShell;
			shargs[1] = buf;
			for (i = 1; (shargs[i + 1] = args[i]) != NULL; ++i) ;
			execve(DefaultShell, shargs, env);
			return;
		case EACCES:
			eaccess = 1;
			break;
		case ENOMEM:
		case E2BIG:
		case ETXTBSY:
			return;
		}
	} while (*path++);
	if (eaccess)
		errno = EACCES;
}
#endif

int winexec(char **av)
{
	char **pp;
	char *p, *s, *t;
	int i, r = 0, l = 0;
	struct win *w;
	struct pseudowin *pwin;
	int type;

	if ((w = display ? fore : windows) == NULL)
		return -1;
	if (!*av || w->w_pwin) {
		Msg(0, "Filter running: %s", w->w_pwin ? w->w_pwin->p_cmd : "(none)");
		return -1;
	}
	if (w->w_ptyfd < 0) {
		Msg(0, "You feel dead inside.");
		return -1;
	}
	if (!(pwin = calloc(1, sizeof(struct pseudowin)))) {
		Msg(0, "%s", strnomem);
		return -1;
	}

	/* allow ^a:!!./ttytest as a short form for ^a:exec !.. ./ttytest */
	for (s = *av; *s == ' '; s++) ;
	for (p = s; *p == ':' || *p == '.' || *p == '!'; p++) ;
	if (*p != '|')
		while (*p && p > s && p[-1] == '.')
			p--;
	if (*p == '|') {
		l = F_UWP;
		p++;
	}
	if (*p)
		av[0] = p;
	else
		av++;

	t = pwin->p_cmd;
	for (i = 0; i < 3; i++) {
		*t = (s < p) ? *s++ : '.';
		switch (*t++) {
		case '.':
		case '|':
			l |= F_PFRONT << (i * F_PSHIFT);
			break;
		case '!':
			l |= F_PBACK << (i * F_PSHIFT);
			break;
		case ':':
			l |= F_PBOTH << (i * F_PSHIFT);
			break;
		}
	}

	if (l & F_UWP) {
		*t++ = '|';
		if ((l & F_PMASK) == F_PFRONT) {
			*pwin->p_cmd = '!';
			l ^= F_PFRONT | F_PBACK;
		}
	}
	if (!(l & F_PBACK))
		l |= F_UWP;
	*t++ = ' ';
	pwin->p_fdpat = l;

	l = MAXSTR - 4;
	for (pp = av; *pp; pp++) {
		p = *pp;
		while (*p && l-- > 0)
			*t++ = *p++;
		if (l <= 0)
			break;
		*t++ = ' ';
	}
	*--t = '\0';

	if ((pwin->p_ptyfd = OpenDevice(av, 0, &type, &t)) < 0) {
		free((char *)pwin);
		return -1;
	}
	strncpy(pwin->p_tty, t, MAXSTR);
	w->w_pwin = pwin;
	if (type != W_TYPE_PTY) {
		FreePseudowin(w);
		Msg(0, "Cannot only use commands as pseudo win.");
		return -1;
	}
	if (!(pwin->p_fdpat & F_PFRONT))
		evdeq(&w->w_readev);
#ifdef TIOCPKT
	{
		int flag = 0;

		if (ioctl(pwin->p_ptyfd, TIOCPKT, (char *)&flag)) {
			Msg(errno, "TIOCPKT pwin ioctl");
			FreePseudowin(w);
			return -1;
		}
		if (w->w_type == W_TYPE_PTY && !(pwin->p_fdpat & F_PFRONT)) {
			if (ioctl(w->w_ptyfd, TIOCPKT, (char *)&flag)) {
				Msg(errno, "TIOCPKT win ioctl");
				FreePseudowin(w);
				return -1;
			}
		}
	}
#endif				/* TIOCPKT */

	pwin->p_readev.fd = pwin->p_writeev.fd = pwin->p_ptyfd;
	pwin->p_readev.type = EV_READ;
	pwin->p_writeev.type = EV_WRITE;
	pwin->p_readev.data = pwin->p_writeev.data = (char *)w;
	pwin->p_readev.handler = pseu_readev_fn;
	pwin->p_writeev.handler = pseu_writeev_fn;
	pwin->p_writeev.condpos = &pwin->p_inlen;
	if (pwin->p_fdpat & (F_PFRONT << F_PSHIFT * 2 | F_PFRONT << F_PSHIFT))
		evenq(&pwin->p_readev);
	evenq(&pwin->p_writeev);
	r = pwin->p_pid = ForkWindow(w, av, t);
	if (r < 0)
		FreePseudowin(w);
	return r;
}

void FreePseudowin(struct win *w)
{
	struct pseudowin *pwin = w->w_pwin;

	if (fcntl(w->w_ptyfd, F_SETFL, FNBLOCK))
		Msg(errno, "Warning: FreePseudowin: NBLOCK fcntl failed");
#ifdef TIOCPKT
	if (w->w_type == W_TYPE_PTY && !(pwin->p_fdpat & F_PFRONT)) {
		int flag = 1;
		if (ioctl(w->w_ptyfd, TIOCPKT, (char *)&flag))
			Msg(errno, "Warning: FreePseudowin: TIOCPKT win ioctl");
	}
#endif
	/* should be able to use CloseDevice() here */
	(void)chmod(pwin->p_tty, 0666);
	(void)chown(pwin->p_tty, 0, 0);
	if (pwin->p_ptyfd >= 0)
		close(pwin->p_ptyfd);
	evdeq(&pwin->p_readev);
	evdeq(&pwin->p_writeev);
	if (w->w_readev.condneg == &pwin->p_inlen)
		w->w_readev.condpos = w->w_readev.condneg = 0;
	evenq(&w->w_readev);
	free((char *)pwin);
	w->w_pwin = NULL;
}

static void paste_slowev_fn(__attribute__((unused))struct event *ev, char *data)
{
	struct paster *pa = (struct paster *)data;
	struct win *p;

	int l = 1;
	flayer = pa->pa_pastelayer;
	if (!flayer)
		pa->pa_pastelen = 0;
	if (!pa->pa_pastelen)
		return;
	p = Layer2Window(flayer);
	DoProcess(p, &pa->pa_pasteptr, &l, pa);
	pa->pa_pastelen -= 1 - l;
	if (pa->pa_pastelen > 0) {
		SetTimeout(&pa->pa_slowev, p->w_slowpaste);
		evenq(&pa->pa_slowev);
	}
}

static int muchpending(struct win *p, struct event *ev)
{
	Canvas *cv;
	for (cv = p->w_layer.l_cvlist; cv; cv = cv->c_lnext) {
		display = cv->c_display;
		if (D_status == STATUS_ON_WIN && !D_status_bell) {
			/* wait 'til status is gone */
			ev->condpos = &const_one;
			ev->condneg = &D_status;
			return 1;
		}
		if (D_blocked)
			continue;
		if (D_obufp - D_obuf > D_obufmax + D_blocked_fuzz) {
			if (D_nonblock == 0) {
				D_blocked = 1;
				continue;
			}
			ev->condpos = &D_obuffree;
			ev->condneg = &D_obuflenmax;
			if (D_nonblock > 0 && !D_blockedev.queued) {
				SetTimeout(&D_blockedev, D_nonblock);
				evenq(&D_blockedev);
			}
			return 1;
		}
	}
	return 0;
}

static void win_readev_fn(struct event *ev, char *data)
{
	struct win *p = (struct win *)data;
	char buf[IOSIZE], *bp;
	int size, len;
	int wtop;

	bp = buf;
	size = IOSIZE;

	wtop = p->w_pwin && W_WTOP(p);
	if (wtop) {
		size = IOSIZE - p->w_pwin->p_inlen;
		if (size <= 0) {
			ev->condpos = &const_IOSIZE;
			ev->condneg = &p->w_pwin->p_inlen;
			return;
		}
	}
	if (p->w_layer.l_cvlist && muchpending(p, ev))
		return;
	if (p->w_blocked) {
		ev->condpos = &const_one;
		ev->condneg = &p->w_blocked;
		return;
	}
	if (ev->condpos)
		ev->condpos = ev->condneg = 0;

	if ((len = p->w_outlen)) {
		p->w_outlen = 0;
		WriteString(p, p->w_outbuf, len);
		return;
	}

	if ((len = read(ev->fd, buf, size)) < 0) {
		if (errno == EINTR || errno == EAGAIN)
			return;
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
		if (errno == EWOULDBLOCK)
			return;
#endif
		WindowDied(p, 0, 0);
		return;
	}
	if (len == 0) {
		WindowDied(p, 0, 0);
		return;
	}
#ifdef TIOCPKT
	if (p->w_type == W_TYPE_PTY) {
		if (buf[0]) {
			if (buf[0] & TIOCPKT_NOSTOP)
				WNewAutoFlow(p, 0);
			if (buf[0] & TIOCPKT_DOSTOP)
				WNewAutoFlow(p, 1);
		}
		bp++;
		len--;
	}
#endif
	if (len == 0)
		return;
	if (wtop) {
		memmove(p->w_pwin->p_inbuf + p->w_pwin->p_inlen, bp, len);
		p->w_pwin->p_inlen += len;
	}

	LayPause(&p->w_layer, 1);
	WriteString(p, bp, len);
	LayPause(&p->w_layer, 0);

	return;
}

static void win_resurrect_zombie_fn(__attribute__((unused))struct event *ev, char *data) {
	struct win *p = (struct win *)data;
	/* Already reconnected? */
	if (p->w_deadpid != p->w_pid)
		return;
	WriteString(p, "\r\n", 2);
	RemakeWindow(p);
}

static void win_writeev_fn(struct event *ev, char *data)
{
	struct win *p = (struct win *)data;
	struct win *win;
	int len;
	if (p->w_inlen) {
		if ((len = write(ev->fd, p->w_inbuf, p->w_inlen)) <= 0)
			len = p->w_inlen;	/* dead window */

		if (p->w_miflag) { /* don't loop if not needed */
			for (win = windows; win; win = win->w_next) {
				if (win != p && win->w_miflag)
					write(win->w_ptyfd, p->w_inbuf, p->w_inlen);
			}
		}

		if ((p->w_inlen -= len))
			memmove(p->w_inbuf, p->w_inbuf + len, p->w_inlen);
	}
	if (p->w_paster.pa_pastelen && !p->w_slowpaste) {
		struct paster *pa = &p->w_paster;
		flayer = pa->pa_pastelayer;
		if (flayer)
			DoProcess(p, &pa->pa_pasteptr, &pa->pa_pastelen, pa);
	}
	return;
}

static void pseu_readev_fn(struct event *ev, char *data)
{
	struct win *p = (struct win *)data;
	char buf[IOSIZE];
	int size, ptow, len;

	size = IOSIZE;

	ptow = W_PTOW(p);
	if (ptow) {
		size = IOSIZE - p->w_inlen;
		if (size <= 0) {
			ev->condpos = &const_IOSIZE;
			ev->condneg = &p->w_inlen;
			return;
		}
	}
	if (p->w_layer.l_cvlist && muchpending(p, ev))
		return;
	if (p->w_blocked) {
		ev->condpos = &const_one;
		ev->condneg = &p->w_blocked;
		return;
	}
	if (ev->condpos)
		ev->condpos = ev->condneg = 0;

	if ((len = p->w_outlen)) {
		p->w_outlen = 0;
		WriteString(p, p->w_outbuf, len);
		return;
	}

	if ((len = read(ev->fd, buf, size)) <= 0) {
		if (errno == EINTR || errno == EAGAIN)
			return;
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
		if (errno == EWOULDBLOCK)
			return;
#endif
		FreePseudowin(p);
		return;
	}
	/* no packet mode on pseudos! */
	if (ptow) {
		memmove(p->w_inbuf + p->w_inlen, buf, len);
		p->w_inlen += len;
	}
	WriteString(p, buf, len);
	return;
}

static void pseu_writeev_fn(struct event *ev, char *data)
{
	struct win *p = (struct win *)data;
	struct pseudowin *pw = p->w_pwin;
	int len;

	if (pw->p_inlen == 0)
		return;
	if ((len = write(ev->fd, pw->p_inbuf, pw->p_inlen)) <= 0)
		len = pw->p_inlen;	/* dead pseudo */
	if ((p->w_pwin->p_inlen -= len))
		memmove(p->w_pwin->p_inbuf, p->w_pwin->p_inbuf + len, p->w_pwin->p_inlen);
}

static void win_silenceev_fn(__attribute__((unused))struct event *ev, char *data)
{
	struct win *p = (struct win *)data;
	Canvas *cv;
	for (display = displays; display; display = display->d_next) {
		for (cv = D_cvlist; cv; cv = cv->c_next)
			if (cv->c_layer->l_bottom == &p->w_layer)
				break;
		if (cv)
			continue;	/* user already sees window */
		Msg(0, "Window %d: silence for %d seconds", p->w_number, p->w_silencewait);
		p->w_silence = SILENCE_FOUND;
		WindowChanged(p, 'f');
	}
}

static void win_destroyev_fn(struct event *ev, __attribute__((unused))char *data)
{
	struct win *p = (struct win *)ev->data;
	WindowDied(p, p->w_exitstatus, 1);
}

int SwapWindows(int old, int dest)
{
	struct win *p, *win_old;

	if (dest < 0 || dest >= maxwin) {
		Msg(0, "Given window position is invalid.");
		return 0;
	}

	win_old = wtab[old];
	p = wtab[dest];
	wtab[dest] = win_old;
	win_old->w_number = dest;
	wtab[old] = p;
	if (p)
		p->w_number = old;
#ifdef UTMPOK
	/* exchange the utmp-slots for these windows */
	if ((win_old->w_slot != (slot_t) - 1) && (win_old->w_slot != (slot_t) 0)) {
		RemoveUtmp(win_old);
		SetUtmp(win_old);
	}
	if (p && (p->w_slot != (slot_t) - 1) && (p->w_slot != (slot_t) 0)) {
		display = win_old->w_layer.l_cvlist ? win_old->w_layer.l_cvlist->c_display : 0;
		RemoveUtmp(p);
		SetUtmp(p);
	}
#endif

	WindowChanged(win_old, 'n');
	WindowChanged((struct win *)0, 'w');
	WindowChanged((struct win *)0, 'W');
	WindowChanged((struct win *)0, 0);
	return 1;
}
