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

#include "attacher.h"

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

#ifdef USE_PAM
#include <security/pam_appl.h>
#endif

#include "misc.h"
#include "socket.h"
#include "tty.h"

static int WriteMessage(int, Message *);
static void AttacherSigInt(int);
static void AttacherWinch(int);
static void DoLock(int);
static void LockTerminal(void);
static void LockHup(int);
static void screen_builtin_lck(void);
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

static int WriteMessage(int s, Message *m)
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
	Message m;
	struct stat st;
	char *s;

	if ((how == MSG_ATTACH || how == MSG_CONT) && multiattach) {
		real_uid = multi_uid;
		eff_uid = own_uid;
#ifdef HAVE_SETRESUID
		if (setresuid(multi_uid, own_uid, multi_uid))
			Panic(errno, "setresuid");
#else
		xseteuid(multi_uid);
		xseteuid(own_uid);
#endif
		if (chmod(attach_tty, 0666))
			Panic(errno, "chmod %s", attach_tty);
		tty_oldmode = tty_mode;
	}

	memset((char *)&m, 0, sizeof(m));
	m.type = how;
	m.protocol_revision = MSG_REVISION;
	strncpy(m.m_tty, attach_tty, sizeof(m.m_tty) - 1);
	m.m_tty[sizeof(m.m_tty) - 1] = 0;

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
	if (!multiattach) {
		if (setuid(real_uid))
			Panic(errno, "setuid");
	} else {
		/* This call to xsetuid should also set the saved uid */
		xseteuid(real_uid);	/* multi_uid, allow backend to send signals */
	}
	if (setgid(real_gid))
		Panic(errno, "setgid");
	eff_uid = real_uid;
	eff_gid = real_gid;

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
		m.m.detach.dpid = getpid();
		strncpy(m.m.detach.duser, LoginName, sizeof(m.m.detach.duser) - 1);
		m.m.detach.duser[sizeof(m.m.detach.duser) - 1] = 0;
		if (dflag == 2)
			m.type = MSG_POW_DETACH;
		else
			m.type = MSG_DETACH;
		/* If there is no password for the session, or the user enters the correct
		 * password, then we get a SIGCONT. Otherwise we get a SIG_BYE */
		xsignal(SIGCONT, AttachSigCont);
		if (WriteMessage(lasts, &m))
			Panic(errno, "WriteMessage");
		close(lasts);
		while (!ContinuePlease)
			pause();	/* wait for SIGCONT */
		xsignal(SIGCONT, SIG_DFL);
		ContinuePlease = 0;
		if (how != MSG_ATTACH)
			return 0;	/* we detached it. jw. */
		sleep(1);	/* we dont want to overrun our poor backend. jw. */
		if ((lasts = MakeClientSocket(0)) == -1)
			Panic(0, "Cannot contact screen again. Sigh.");
		m.type = how;
	}
	strncpy(m.m.attach.envterm, attach_term, MAXTERMLEN);
	m.m.attach.envterm[MAXTERMLEN] = 0;

	strncpy(m.m.attach.auser, LoginName, sizeof(m.m.attach.auser) - 1);
	m.m.attach.auser[sizeof(m.m.attach.auser) - 1] = 0;
	m.m.attach.esc = DefaultEsc;
	m.m.attach.meta_esc = DefaultMetaEsc;
	strncpy(m.m.attach.preselect, preselect ? preselect : "", sizeof(m.m.attach.preselect) - 1);
	m.m.attach.preselect[sizeof(m.m.attach.preselect) - 1] = 0;
	m.m.attach.apid = getpid();
	m.m.attach.adaptflag = adaptflag;
	m.m.attach.lines = m.m.attach.columns = 0;
	if ((s = getenv("LINES")))
		m.m.attach.lines = atoi(s);
	if ((s = getenv("COLUMNS")))
		m.m.attach.columns = atoi(s);

	if (dflag == 2)
		m.m.attach.detachfirst = MSG_POW_DETACH;
	else if (dflag)
		m.m.attach.detachfirst = MSG_DETACH;
	else
		m.m.attach.detachfirst = MSG_ATTACH;

	/* setup CONT signal handler to repair the terminal mode */
	if (multi && (how == MSG_ATTACH || how == MSG_CONT))
		xsignal(SIGCONT, AttachSigCont);

	if (WriteMessage(lasts, &m))
		Panic(errno, "WriteMessage");
	close(lasts);
	if (multi && (how == MSG_ATTACH || how == MSG_CONT)) {
		while (!ContinuePlease)
			pause();	/* wait for SIGCONT */
		xsignal(SIGCONT, SIG_DFL);
		ContinuePlease = 0;
		xseteuid(own_uid);
		if (tty_oldmode >= 0)
			if (chmod(attach_tty, tty_oldmode))
				Panic(errno, "chmod %s", attach_tty);
		tty_oldmode = -1;
		xseteuid(real_uid);
	}
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
	(void)sigsig; /* unused */

	xsignal(SIGINT, AttacherSigInt);
	Kill(MasterPid, SIGINT);
}

/*
 * Unfortunately this is also the SIGHUP handler, so we have to
 * check if the backend is already detached.
 */

void AttacherFinit(int sigsig)
{
	struct stat statb;
	Message m;
	int s;

	(void)sigsig; /* unused */

	xsignal(SIGHUP, SIG_IGN);
	/* Check if signal comes from backend */
	if (stat(SocketPath, &statb) == 0 && (statb.st_mode & 0777) != 0600) {
		memset((char *)&m, 0, sizeof(m));
		strncpy(m.m_tty, attach_tty, sizeof(m.m_tty) - 1);
		m.m_tty[sizeof(m.m_tty) - 1] = 0;
		m.m.detach.dpid = getpid();
		m.type = MSG_HANGUP;
		m.protocol_revision = MSG_REVISION;
		if ((s = MakeClientSocket(0)) >= 0) {
			WriteMessage(s, &m);
			close(s);
		}
	}
	if (tty_oldmode >= 0) {
		if (setuid(own_uid))
			Panic(errno, "setuid");
		chmod(attach_tty, tty_oldmode);
	}
	exit(0);
}

static void AttacherFinitBye(int sigsig)
{
	int ppid;

	(void)sigsig; /* unused */

	if (setgid(real_gid))
		Panic(errno, "setgid");
	if (setuid(own_uid))
		Panic(errno, "setuid");

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
	xsignal(SIGHUP, AttacherFinit);
	xsignal(SIG_BYE, AttacherFinit);
	xsignal(SIG_POWER_BYE, AttacherFinitBye);
	xsignal(SIG_LOCK, DoLock);
	xsignal(SIGINT, AttacherSigInt);
	xsignal(SIG_STOP, SigStop);
	xsignal(SIGWINCH, AttacherWinch);
	dflag = 0;
	xflag = 1;
	for (;;) {
		xsignal(SIGALRM, AttacherSigAlarm);
		alarm(15);
		pause();
		alarm(0);
		if (kill(MasterPid, 0) < 0 && errno != EPERM) {
			AttacherPanic++;
		}
		if (AttacherPanic) {
			fcntl(0, F_SETFL, 0);
			SetTTY(0, &attach_Mode);
			printf("\nError: Cannot find master process to attach to!\n");
			eexit(1);
		}
		if (SuspendPlease) {
			SuspendPlease = 0;
			xsignal(SIGTSTP, SIG_DFL);
			kill(getpid(), SIGTSTP);
			xsignal(SIG_STOP, SigStop);
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

	if (setgid(real_gid))
		Panic(errno, "setgid");
	if (setuid(own_uid))
		Panic(errno, "setuid");
	if (ppid > 1)
		Kill(ppid, SIGHUP);
	exit(0);
}

static void LockTerminal()
{
	int sig;
	void (*sigs[_NSIG - 1]) (int);

	for (sig = 1; sig < _NSIG - 1; sig++)
		sigs[sig] = xsignal(sig, sig == SIGCHLD ? SIG_DFL : SIG_IGN);
	xsignal(SIGHUP, LockHup);
	printf("\n");

	screen_builtin_lck();

	/* reset signals */
	for (sig = 1; sig < _NSIG - 1; sig++) {
		if (sigs[sig] != (void (*)(int))-1)
			xsignal(sig, sigs[sig]);
	}
}				/* LockTerminal */

#ifdef USE_PAM

static int PAM_conv(int, const struct pam_message **, struct pam_response **, void *);

static int PAM_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
	int replies = 0;
	struct pam_response *reply = NULL;

	reply = malloc(sizeof(struct pam_response) * num_msg);
	if (!reply)
		return PAM_CONV_ERR;
#define COPY_STRING(s) (s) ? strdup(s) : NULL

	for (replies = 0; replies < num_msg; replies++) {
		switch (msg[replies]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
			/* wants password */
			reply[replies].resp_retcode = PAM_SUCCESS;
			reply[replies].resp = appdata_ptr ? strdup((char *)appdata_ptr) : 0;
			break;
		case PAM_TEXT_INFO:
			/* ignore the informational mesage */
			/* but first clear out any drek left by malloc */
			reply[replies].resp = NULL;
			break;
		case PAM_PROMPT_ECHO_ON:
			/* user name given to PAM already */
			/* fall through */
		default:
			/* unknown or PAM_ERROR_MSG */
			free(reply);
			return PAM_CONV_ERR;
		}
	}
	*resp = reply;
	return PAM_SUCCESS;
}

static struct pam_conv PAM_conversation = {
	&PAM_conv,
	NULL
};

#endif

/* -- original copyright by Luigi Cannelloni 1985 (luigi@faui70.UUCP) -- */
static void screen_builtin_lck()
{
	char fullname[100], *cp1, message[100 + 100];
#ifdef USE_PAM
	pam_handle_t *pamh = 0;
	int pam_error;
	char *tty_name;
#endif
	char *pass = 0, mypass[16 + 1], salt[3];
	int using_pam = 1;

#ifdef USE_PAM
	if (!ppp->pw_uid) {
#endif
		using_pam = 0;
		pass = ppp->pw_passwd;
		if (pass == 0 || *pass == 0) {
			if ((pass = getpass("Key:   "))) {
				strncpy(mypass, pass, sizeof(mypass) - 1);
				mypass[sizeof(mypass) - 1] = 0;
				if (*mypass == 0)
					return;
				if ((pass = getpass("Again: "))) {
					if (strcmp(mypass, pass)) {
						fprintf(stderr, "Passwords don't match.\007\n");
						sleep(2);
						return;
					}
				}
			}
			if (pass == 0) {
				fprintf(stderr, "Getpass error.\007\n");
				sleep(2);
				return;
			}

			salt[0] = 'A' + (int)(time(0) % 26);
			salt[1] = 'A' + (int)((time(0) >> 6) % 26);
			salt[2] = 0;
			pass = crypt(mypass, salt);
			if (!pass) {
				fprintf(stderr, "crypt() error.\007\n");
				sleep(2);
				return;
			}
			pass = ppp->pw_passwd = SaveStr(pass);
		}
#ifdef USE_PAM
	}
#endif

	strncpy(fullname, ppp->pw_gecos, sizeof(fullname) - 9);
	fullname[sizeof(fullname) - 9] = 0;

	if ((cp1 = strchr(fullname, ',')) != NULL)
		*cp1 = '\0';
	if ((cp1 = strchr(fullname, '&')) != NULL) {
		strncpy(cp1, ppp->pw_name, 8);
		cp1[8] = 0;
		if (*cp1 >= 'a' && *cp1 <= 'z')
			*cp1 -= 'a' - 'A';
	}

	sprintf(message, "Screen used by %s%s<%s> on %s.\nPassword:\007",
		fullname, fullname[0] ? " " : "", ppp->pw_name, HostName);

	/* loop here to wait for correct password */
	for (;;) {
		errno = 0;
		if ((cp1 = getpass(message)) == NULL) {
			AttacherFinit(0);
			/* NOTREACHED */
		}
		if (using_pam) {
#ifdef USE_PAM
			PAM_conversation.appdata_ptr = cp1;
			pam_error = pam_start("screen", ppp->pw_name, &PAM_conversation, &pamh);
			if (pam_error != PAM_SUCCESS)
				AttacherFinit(0);	/* goodbye */

			if (strncmp(attach_tty, "/dev/", 5) == 0)
				tty_name = attach_tty + 5;
			else
				tty_name = attach_tty;
			pam_error = pam_set_item(pamh, PAM_TTY, tty_name);
			if (pam_error != PAM_SUCCESS)
				AttacherFinit(0);	/* goodbye */

			pam_error = pam_authenticate(pamh, 0);
			pam_end(pamh, pam_error);
			PAM_conversation.appdata_ptr = 0;
			if (pam_error == PAM_SUCCESS)
				break;
#endif
		} else {
			char *buf = crypt(cp1, pass);
			if (buf && !strncmp(buf, pass, strlen(pass)))
				break;
		}
		memset(cp1, 0, strlen(cp1));
	}
	memset(cp1, 0, strlen(cp1));
}

void SendCmdMessage(char *sty, char *match, char **av, int query)
{
	int i, s;
	Message m;
	char *p;
	int n;

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
		m.m_tty[sizeof(m.m_tty) - 1] = 0;
	}
	p = m.m.command.cmd;
	n = 0;
	for (; *av && n < MAXARGS - 1; ++av, ++n) {
		size_t len;
		len = strlen(*av) + 1;
		if (p + len >= m.m.command.cmd + sizeof(m.m.command.cmd) - 1)
			break;
		strncpy(p, *av, MAXPATHLEN);
		p += len;
	}
	*p = 0;
	m.m.command.nargs = n;
	strncpy(m.m.attach.auser, LoginName, sizeof(m.m.attach.auser) - 1);
	m.m.command.auser[sizeof(m.m.command.auser) - 1] = 0;
	m.protocol_revision = MSG_REVISION;
	strncpy(m.m.command.preselect, preselect ? preselect : "", sizeof(m.m.command.preselect) - 1);
	m.m.command.preselect[sizeof(m.m.command.preselect) - 1] = 0;
	m.m.command.apid = getpid();
	if (query) {
		/* Create a server socket so we can get back the result */
		char *sp = SocketPath + strlen(SocketPath);
		char query[] = "-queryX";
		char c;
		int r = -1;
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
		m.m.command.writeback[sizeof(m.m.command.writeback) - 1] = '\0';

		/* Send the message, then wait for a response */
		xsignal(SIGCONT, QueryResultSuccess);
		xsignal(SIG_BYE, QueryResultFail);
		if (WriteMessage(s, &m))
			Msg(errno, "write");
		close(s);
		while (!QueryResult)
			pause();
		xsignal(SIGCONT, SIG_DFL);
		xsignal(SIG_BYE, SIG_DFL);

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
