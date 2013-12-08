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

#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "screen.h"

#include "authentication.h"
#include "misc.h"
#include "socket.h"
#include "tty.h"

static int WriteMessage(int, struct msg *);
static void AttacherSigInt(int);
static void AttacherWinch(int);
static void DoLock(int);
static void LockTerminal(void);
static void LockHup(int);
static void AttachSigCont(int);
static int ContinuePlease;

static void AttachSigCont(int sigsig)
{
	(void)sigsig; /* unused */
	ContinuePlease = 1;
}

static int QueryResult;

static void QueryResultSuccess(int sigsig)
{
	(void)sigsig; /* unused */
	QueryResult = 1;
}

static void QueryResultFail(int sigsig)
{
	(void)sigsig; /* unused */
	QueryResult = 2;
}

/*
 *  Send message to a screen backend.
 *  returns 1 if we could attach one, or 0 if none.
 *  Understands  MSG_ATTACH, MSG_DETACH, MSG_POW_DETACH
 *               MSG_CONT, MSG_WINCH and nothing else!
 *
 *  if type == MSG_ATTACH and sockets are used, attaches
 *  tty filedescriptor.
 */

static int WriteMessage(int s, struct msg *m)
{
	int r, l = sizeof(*m);

	if (m->type == MSG_ATTACH)
		return SendAttachMsg(s, m, attach_fd);

	while (l > 0) {
		r = write(s, (char *)m + (sizeof(*m) - l), l);
		if (r == -1 && errno == EINTR)
			continue;
		if (r == -1 || r == 0)
			return -1;
		l -= r;
	}
	return 0;
}

int Attach(int how)
{
	int n, lasts;
	struct msg m;
	struct stat st;
	char *s;

	memset((char *)&m, 0, sizeof(m));
	m.type = how;
	m.protocol_revision = MSG_REVISION;
	strncpy(m.m_tty, attach_tty, sizeof(m.m_tty) - 1);

	if (how == MSG_WINCH) {
		if ((lasts = MakeClientSocket(0)) >= 0) {
			WriteMessage(lasts, &m);
			close(lasts);
		}
		return 0;
	}

	if (how == MSG_CONT) {
		if ((lasts = MakeClientSocket(0)) < 0) {
			Panic(0, "Sorry, cannot contact session \"%s\" again.\r\n", SocketName);
		}
	} else {
		n = FindSocket(&lasts, (int *)0, (int *)0, SocketMatch);
		switch (n) {
		case 0:
			if (rflag && (rflag & 1) == 0)
				return 0;
			if (quietflag)
				eexit(10);
			if (SocketMatch && *SocketMatch) {
				Panic(0, "There is no screen to be %sed matching %s.",
				      xflag ? "attach" : dflag ? "detach" : "resum", SocketMatch);
			} else {
				Panic(0, "There is no screen to be %sed.",
				      xflag ? "attach" : dflag ? "detach" : "resum");
			}
			/* NOTREACHED */
		case 1:
			break;
		default:
			if (rflag < 3) {
				if (quietflag)
					eexit(10 + n);
				Panic(0, "Type \"screen [-d] -r [pid.]tty.host\" to resume one of them.");
			}
			/* NOTREACHED */
		}
	}
	/*
	 * Go in UserContext. Advantage is, you can kill your attacher
	 * when things go wrong. Any disadvantages? jw.
	 * Do this before the attach to prevent races!
	 */

	LockTerminal();

	MasterPid = 0;
	for (s = SocketName; *s; s++) {
		if (*s > '9' || *s < '0')
			break;
		MasterPid = 10 * MasterPid + (*s - '0');
	}
	if (stat(SocketPath, &st) == -1)
		Panic(errno, "stat %s", SocketPath);
	if ((st.st_mode & 0600) != 0600)
		Panic(0, "Socket is in wrong mode (%03o)", (int)st.st_mode);

	/*
	 * Change: if -x or -r ignore failing -d
	 */
	if ((xflag || rflag) && dflag && (st.st_mode & 0700) == 0600)
		dflag = 0;

	/*
	 * Without -x, the mode must match.
	 * With -x the mode is irrelevant unless -d.
	 */
	if ((dflag || !xflag) && (st.st_mode & 0700) != (dflag ? 0700 : 0600))
		Panic(0, "That screen is %sdetached.", dflag ? "already " : "not ");
	if (dflag && (how == MSG_DETACH || how == MSG_POW_DETACH)) {
		struct sigaction sigact;
		sigemptyset (&sigact.sa_mask);
		sigact.sa_flags = 0;

		m.m.detach.dpid = getpid();
		strncpy(m.m.detach.duser, LoginName, sizeof(m.m.detach.duser) - 1);
		if (dflag == 2)
			m.type = MSG_POW_DETACH;
		else
			m.type = MSG_DETACH;

		/* If there is no password for the session, or the user enters the correct
		 * password, then we get a SIGCONT. Otherwise we get a SIG_BYE */
		sigact.sa_handler = AttachSigCont;
		sigaction(SIGCONT, &sigact, NULL);
		if (WriteMessage(lasts, &m))
			Panic(errno, "WriteMessage");
		close(lasts);
		while (!ContinuePlease)
			pause();	/* wait for SIGCONT */

		sigact.sa_handler = SIG_DFL;
		sigaction(SIGCONT, &sigact, NULL);
		ContinuePlease = 0;
		if (how != MSG_ATTACH)
			return 0;	/* we detached it. jw. */
		sleep(1);	/* we dont want to overrun our poor backend. jw. */
		if ((lasts = MakeClientSocket(0)) == -1)
			Panic(0, "Cannot contact screen again. Sigh.");
		m.type = how;
	}
	strncpy(m.m.attach.envterm, attach_term, sizeof(m.m.attach.envterm) - 1);

	strncpy(m.m.attach.auser, LoginName, sizeof(m.m.attach.auser) - 1);
	m.m.attach.esc = DefaultEsc;
	m.m.attach.meta_esc = DefaultMetaEsc;
	strncpy(m.m.attach.preselect, preselect ? preselect : "", sizeof(m.m.attach.preselect) - 1);
	m.m.attach.apid = getpid();
	m.m.attach.adaptflag = adaptflag;
	m.m.attach.lines = m.m.attach.columns = 0;
	if ((s = getenv("LINES")))
		m.m.attach.lines = atoi(s);
	if ((s = getenv("COLUMNS")))
		m.m.attach.columns = atoi(s);
	m.m.attach.encoding = nwin_options.encoding > 0 ? nwin_options.encoding + 1 : 0;

	if (dflag == 2)
		m.m.attach.detachfirst = MSG_POW_DETACH;
	else if (dflag)
		m.m.attach.detachfirst = MSG_DETACH;
	else
		m.m.attach.detachfirst = MSG_ATTACH;

	if (WriteMessage(lasts, &m))
		Panic(errno, "WriteMessage");
	close(lasts);
	rflag = 0;
	return 1;
}

static int AttacherPanic = 0;

static void AttacherSigAlarm(int sigsig)
{
	(void)sigsig; /* unused */
}

/*
 * the frontend's Interrupt handler
 * we forward SIGINT to the poor backend
 */
static void AttacherSigInt(int sigsig)
{
	struct sigaction sigact;

	(void)sigsig; /* unused */

	sigemptyset (&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = AttacherSigInt;
	sigaction(SIGINT, &sigact, NULL);
	Kill(MasterPid, SIGINT);
}

/*
 * Unfortunately this is also the SIGHUP handler, so we have to
 * check if the backend is already detached.
 */

void AttacherFinit(int sigsig)
{
	struct stat statb;
	struct msg m;
	int s;
	struct sigaction sigact;

	(void)sigsig; /* unused */

	sigemptyset (&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &sigact, NULL);
	/* Check if signal comes from backend */
	if (stat(SocketPath, &statb) == 0 && (statb.st_mode & 0777) != 0600) {
		memset((char *)&m, 0, sizeof(m));
		strncpy(m.m_tty, attach_tty, sizeof(m.m_tty) - 1);
		m.m.detach.dpid = getpid();
		m.type = MSG_HANGUP;
		m.protocol_revision = MSG_REVISION;
		if ((s = MakeClientSocket(0)) >= 0) {
			WriteMessage(s, &m);
			close(s);
		}
	}
	exit(0);
}

static void AttacherFinitBye(int sigsig)
{
	int ppid;

	(void)sigsig; /* unused */

	/* we don't want to disturb init (even if we were root), eh? jw */
	if ((ppid = getppid()) > 1)
		Kill(ppid, SIGHUP);	/* carefully say good bye. jw. */
	exit(0);
}

static int SuspendPlease;

static void SigStop(int sigsig)
{
	(void)sigsig; /* unused */

	SuspendPlease = 1;
}

static int LockPlease;

static void DoLock(int sigsig)
{
	(void)sigsig; /* unused */

	LockPlease = 1;
}

static int SigWinchPlease;

static void AttacherWinch(int sigsig)
{
	(void)sigsig; /* unused */

	SigWinchPlease = 1;
}

/*
 *  Attacher loop - no return
 */

void Attacher()
{
	struct sigaction sigact;

	sigemptyset (&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = AttacherFinit;
	sigaction(SIGHUP, &sigact, NULL);
	sigaction(SIG_BYE, &sigact, NULL);

	sigact.sa_handler = AttacherFinitBye;
	sigaction(SIG_POWER_BYE, &sigact, NULL);

	sigact.sa_handler = DoLock;
	sigaction(SIG_LOCK, &sigact, NULL);
	
	sigact.sa_handler = AttacherSigInt;
	sigaction(SIGINT, &sigact, NULL);
	
	sigact.sa_handler = SigStop;
	sigaction(SIG_STOP, &sigact, NULL);

	sigact.sa_handler = AttacherWinch;
	sigaction(SIGWINCH, &sigact, NULL);

	dflag = 0;
	xflag = 1;
	for (;;) {
		sigact.sa_handler = AttacherSigAlarm;
		sigaction(SIGALRM, &sigact, NULL);
		alarm(15);
		pause();
		alarm(0);
		if (kill(MasterPid, 0) < 0 && errno != EPERM) {
			AttacherPanic++;
		}
		if (AttacherPanic) {
			fcntl(0, F_SETFL, 0);
			SetTTY(0, &attach_Mode);
			printf("\nSuddenly the Dungeon collapses!! - You die...\n");
			eexit(1);
		}
		if (SuspendPlease) {
			SuspendPlease = 0;

			sigact.sa_handler = SIG_DFL;
			sigaction(SIGTSTP, &sigact, NULL);
			kill(getpid(), SIGTSTP);

			sigact.sa_handler = SigStop;
			sigaction(SIG_STOP, &sigact, NULL);
			(void)Attach(MSG_CONT);
		}
		if (LockPlease) {
			LockPlease = 0;
			LockTerminal();
			(void)Attach(MSG_CONT);
		}
		if (SigWinchPlease) {
			SigWinchPlease = 0;
			(void)Attach(MSG_WINCH);
		}
	}
}

/* ADDED by Rainer Pruy 10/15/87 */
/* POLISHED by mls. 03/10/91 */

static void LockHup(int sigsig)
{
	int ppid = getppid();

	(void)sigsig; /* unused */

	if (ppid > 1)
		Kill(ppid, SIGHUP);
	exit(0);
}

static void LockTerminal()
{
	int sig;

	struct sigaction sigact, sigold[_NSIG];
	sigemptyset (&sigact.sa_mask);
	sigact.sa_flags = 0;

	for (sig = 1; sig < _NSIG; sig++) {
		sigact.sa_handler = (sig == SIGCHLD ? SIG_DFL : SIG_IGN);
		if (sigaction(sig, &sigact, &sigold[sig])) {
			sigold[sig].sa_handler = (void (*)(int))-1;
		}
	}

	sigact.sa_handler = LockHup;
	sigaction(SIGHUP, &sigact, NULL);
	printf("\n");

	Authenticate();

	/* reset signals */
	for (sig = 1; sig < _NSIG; sig++) {
		if (sigold[sig].sa_handler != (void (*)(int))-1)
			sigaction(sig, &sigold[sig], NULL);
	}
}				/* LockTerminal */

void SendCmdMessage(char *sty, char *match, char **av, int query)
{
	int i, s;
	struct msg m;
	char *p;
	int len, n;

	if (sty == 0) {
		i = FindSocket(&s, (int *)0, (int *)0, match);
		if (i == 0)
			Panic(0, "No screen session found.");
		if (i != 1)
			Panic(0, "Use -S to specify a session.");
	} else {
		if (strlen(sty) > FILENAME_MAX)
			sty[FILENAME_MAX] = 0;
		if (strlen(sty) > 2 * MAXSTR - 1)
			sty[2 * MAXSTR - 1] = 0;
		sprintf(SocketPath + strlen(SocketPath), "/%s", sty);
		if ((s = MakeClientSocket(1)) == -1)
			exit(1);
	}
	memset((char *)&m, 0, sizeof(m));
	m.type = query ? MSG_QUERY : MSG_COMMAND;
	if (attach_tty) {
		strncpy(m.m_tty, attach_tty, sizeof(m.m_tty) - 1);
	}
	p = m.m.command.cmd;
	n = 0;
	for (; *av && n < MAXARGS - 1; ++av, ++n) {
		len = strlen(*av) + 1;
		if (p + len >= m.m.command.cmd + sizeof(m.m.command.cmd) - 1)
			break;
		strncpy(p, *av, MAXPATHLEN);
		p += len;
	}
	*p = 0;
	m.m.command.nargs = n;
	strncpy(m.m.attach.auser, LoginName, sizeof(m.m.attach.auser) - 1);
	m.protocol_revision = MSG_REVISION;
	strncpy(m.m.command.preselect, preselect ? preselect : "", sizeof(m.m.command.preselect) - 1);
	m.m.command.apid = getpid();
	if (query) {
		/* Create a server socket so we can get back the result */
		char *sp = SocketPath + strlen(SocketPath);
		char query[] = "-queryX";
		char c;
		int r = -1;
		struct sigaction sigact;

		for (c = 'A'; c <= 'Z'; c++) {
			query[6] = c;
			strncpy(sp, query, strlen(SocketPath));
			if ((r = MakeServerSocket()) >= 0)
				break;
		}
		if (r < 0) {
			for (c = '0'; c <= '9'; c++) {
				query[6] = c;
				strncpy(sp, query, strlen(SocketPath));
				if ((r = MakeServerSocket()) >= 0)
					break;
			}
		}

		if (r < 0)
			Panic(0, "Could not create a listening socket to read the results.");

		strncpy(m.m.command.writeback, SocketPath, sizeof(m.m.command.writeback) - 1);

		/* Send the message, then wait for a response */
		sigemptyset (&sigact.sa_mask);
		sigact.sa_flags = 0;
		sigact.sa_handler = QueryResultSuccess;
		sigaction(SIGCONT, &sigact, NULL);
		sigact.sa_handler = QueryResultFail;
		sigaction(SIG_BYE, &sigact, NULL);
		if (WriteMessage(s, &m))
			Msg(errno, "write");
		close(s);
		while (!QueryResult)
			pause();
		sigact.sa_handler = SIG_DFL;
		sigaction(SIGCONT, &sigact, NULL);
		sigact.sa_handler = SIG_DFL;
		sigaction(SIG_BYE, &sigact, NULL);

		/* Read the result and spit it out to stdout */
		ReceiveRaw(r);
		unlink(SocketPath);
		if (QueryResult == 2)	/* An error happened */
			exit(1);
	} else {
		if (WriteMessage(s, &m))
			Msg(errno, "write");
		close(s);
	}
}
