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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <utime.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>

#include "screen.h"

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#else
#include <sys/dir.h>
#define dirent direct
#endif

#include "extern.h"

#include "encoding.h"
#include "fileio.h"
#include "list_generic.h"
#include "misc.h"
#include "process.h"
#include "resize.h"
#include "socket.h"
#include "termcap.h"
#include "tty.h"
#include "utmp.h"

static int CheckPid(int);
static void ExecCreate(struct msg *);
static void DoCommandMsg(struct msg *);
static void FinishAttach(struct msg *);
static void FinishDetach(struct msg *);

#define SOCKMODE (S_IWRITE | S_IREAD | (displays ? S_IEXEC : 0))

/*
 *  Socket directory manager
 *
 *  fdp: pointer to store the first good socket.
 *  nfoundp: pointer to store the number of sockets found matching.
 *  notherp: pointer to store the number of sockets not matching.
 *  match: string to match socket name.
 *
 *  The socket directory must be in SocketPath!
 *  The global variables LoginName, multi, rflag, xflag, dflag,
 *  quietflag, SocketPath are used.
 *
 *  The first good socket is stored in fdp and its name is
 *  appended to SocketPath.
 *  If none exists or fdp is NULL SocketPath is not changed.
 *
 *  Returns: number of good sockets.
 *
 */

int FindSocket(int *fdp, int *nfoundp, int *notherp, char *match)
{
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	int mode;
	int sdirlen;
	int matchlen = 0;
	char *name, *n;
	int firsts = -1, sockfd;
	char *firstn = NULL;
	int nfound = 0, ngood = 0, ndead = 0, nwipe = 0, npriv = 0;
	int nperfect = 0;
	struct sent {
		struct sent *next;
		int mode;
		char *name;
	} *slist, **slisttail, *sent, *nsent;

	if (match) {
		matchlen = strlen(match);
		if (matchlen > FILENAME_MAX)
			matchlen = FILENAME_MAX;
	}

	/*
	 * SocketPath contains the socket directory.
	 * At the end of FindSocket the socket name will be appended to it.
	 * Thus FindSocket() can only be called once!
	 */
	sdirlen = strlen(SocketPath);

	if ((dirp = opendir(SocketPath)) == 0)
		Panic(errno, "Cannot opendir %s", SocketPath);

	slist = 0;
	slisttail = &slist;
	while ((dp = readdir(dirp))) {
		int cmatch = 0;
		name = dp->d_name;
		if (*name == 0 || *name == '.' || strlen(name) > 2 * MAXSTR)
			continue;
		if (matchlen) {
			n = name;
			/* if we don't want to match digits. Skip them */
			if ((*match <= '0' || *match > '9') && (*n > '0' && *n <= '9')) {
				while (*n >= '0' && *n <= '9')
					n++;
				if (*n == '.')
					n++;
			}
			/* the tty prefix is optional */
			if (strncmp(match, "tty", 3) && strncmp(n, "tty", 3) == 0)
				n += 3;
			if (strncmp(match, n, matchlen)) {
				if (n == name && *match > '0' && *match <= '9') {
					while (*n >= '0' && *n <= '9')
						n++;
					if (*n == '.')
						n++;
					if (strncmp(match, n, matchlen))
						continue;
				} else
					continue;
			} else
				cmatch = (*(n + matchlen) == 0);
		}
		sprintf(SocketPath + sdirlen, "/%s", name);

		errno = 0;
		if (stat(SocketPath, &st)) {
			continue;
		}
#ifndef SOCK_NOT_IN_FS
#else
#ifdef S_ISSOCK
		if (!S_ISSOCK(st.st_mode))
			continue;
#endif
#endif

		if (st.st_uid != real_uid)
			continue;
		mode = (int)st.st_mode & 0777;
		if ((sent = malloc(sizeof(struct sent))) == 0)
			continue;
		sent->next = 0;
		sent->name = SaveStr(name);
		sent->mode = mode;
		*slisttail = sent;
		slisttail = &sent->next;
		nfound++;
		sockfd = MakeClientSocket(0);
		if (sockfd == -1) {
			sent->mode = -3;
			ndead++;
			sent->mode = -1;
			if (wipeflag) {
				if (unlink(SocketPath) == 0) {
					sent->mode = -2;
					nwipe++;
				}
			}
			continue;
		}

		mode &= 0776;
		/* Shall we connect ? */

		/*
		 * mode 600: socket is detached.
		 * mode 700: socket is attached.
		 * xflag implies rflag here.
		 *
		 * fail, when socket mode mode is not 600 or 700
		 * fail, when we want to detach w/o reattach, but it already is detached.
		 * fail, when we only want to attach, but mode 700 and not xflag.
		 * fail, if none of dflag, rflag, xflag is set.
		 */
		if ((mode != 0700 && mode != 0600) ||
		    (dflag && !rflag && !xflag && mode == 0600) ||
		    (!dflag && rflag && mode == 0700 && !xflag) || (!dflag && !rflag && !xflag)) {
			close(sockfd);
			npriv++;	/* a good socket that was not for us */
			continue;
		}
		ngood++;
		if (cmatch)
			nperfect++;
		if (fdp && (firsts == -1 || (cmatch && nperfect == 1))) {
			if (firsts != -1)
				close(firsts);
			firsts = sockfd;
			firstn = sent->name;
		} else {
			close(sockfd);
		}
	}
	(void)closedir(dirp);
	if (!lsflag && nperfect == 1)
		ngood = nperfect;
	if (nfound && (lsflag || ngood != 1) && !quietflag) {
		switch (ngood) {
		case 0:
			Msg(0, nfound > 1 ? "There are screens on:" : "There is a screen on:");
			break;
		case 1:
			Msg(0, nfound > 1 ? "There are several screens on:" : "There is a suitable screen on:");
			break;
		default:
			Msg(0, "There are several suitable screens on:");
			break;
		}
		for (sent = slist; sent; sent = sent->next) {
			switch (sent->mode) {
			case 0700:
				printf("\t%s\t(Attached)\n", sent->name);
				break;
			case 0600:
				printf("\t%s\t(Detached)\n", sent->name);
				break;
			case -1:
				/* No trigraphs here! */
				printf("\t%s\t(Dead ?%c?)\n", sent->name, '?');
				break;
			case -2:
				printf("\t%s\t(Removed)\n", sent->name);
				break;
			case -3:
				printf("\t%s\t(Remote or dead)\n", sent->name);
				break;
			case -4:
				printf("\t%s\t(Private)\n", sent->name);
				break;
			}
		}
	}
	if (ndead && !quietflag) {
		char *m = "Remove dead screens with 'screen -wipe'.";
		if (wipeflag)
			Msg(0, "%d socket%s wiped out.", nwipe, nwipe > 1 ? "s" : "");
		else
			Msg(0, m, ndead > 1 ? "s" : "", ndead > 1 ? "" : "es");
	}
	if (firsts != -1) {
		sprintf(SocketPath + sdirlen, "/%s", firstn);
		*fdp = firsts;
	} else
		SocketPath[sdirlen] = 0;
	for (sent = slist; sent; sent = nsent) {
		nsent = sent->next;
		free(sent->name);
		free((char *)sent);
	}
	if (notherp)
		*notherp = npriv;
	if (nfoundp)
		*nfoundp = nfound - nwipe;
	return ngood;
}

/*
**
**        Socket/pipe create routines
**
*/

int MakeServerSocket()
{
	register int s;
	struct sockaddr_un a;
	struct stat st;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		Panic(errno, "socket");
	a.sun_family = AF_UNIX;
	strncpy(a.sun_path, SocketPath, sizeof(a.sun_path));
	if (connect(s, (struct sockaddr *)&a, strlen(SocketPath) + 2) != -1) {
		if (quietflag) {
			Kill(D_userpid, SIG_BYE);
			/*
			 * oh, well. nobody receives that return code. papa
			 * dies by signal.
			 */
			eexit(11);
		}
		Msg(0, "There is already a screen running on %s.", Filename(SocketPath));
		if (stat(SocketPath, &st) == -1)
			Panic(errno, "stat");
		if (st.st_uid != real_uid)
			Panic(0, "Unfortunately you are not its owner.");
		if ((st.st_mode & 0700) == 0600)
			Panic(0, "To resume it, use \"screen -r\"");
		else
			Panic(0, "It is not detached.");
		/* NOTREACHED */
	}
	(void)unlink(SocketPath);
	if (bind(s, (struct sockaddr *)&a, strlen(SocketPath) + 2) == -1)
		Panic(errno, "bind (%s)", SocketPath);
#ifdef SOCK_NOT_IN_FS
	{
		int f;
		if ((f = open(SocketPath, O_RDWR | O_CREAT, SOCKMODE)) < 0)
			Panic(errno, "shadow socket open");
		close(f);
	}
#else
	chmod(SocketPath, SOCKMODE);
	chown(SocketPath, real_uid, real_gid);
#endif				/* SOCK_NOT_IN_FS */
	if (listen(s, 5) == -1)
		Panic(errno, "listen");
#ifdef F_SETOWN
	fcntl(s, F_SETOWN, getpid());
#endif				/* F_SETOWN */
	return s;
}

int MakeClientSocket(int err)
{
	register int s;
	struct sockaddr_un a;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		Panic(errno, "socket");
	a.sun_family = AF_UNIX;
	strncpy(a.sun_path, SocketPath, sizeof(a.sun_path));
	if (connect(s, (struct sockaddr *)&a, strlen(SocketPath) + 2) == -1) {
		if (err)
			Msg(errno, "%s: connect", SocketPath);
		close(s);
		s = -1;
	}
	return s;
}

/*
**
**       Message send and receive routines
**
*/

void SendCreateMsg(char *sty, struct NewWindow *nwin)
{
	int s;
	struct msg m;
	register char *p;
	register int len, n;
	char **av;

	if (strlen(sty) > FILENAME_MAX)
		sty[FILENAME_MAX] = 0;
	if (strlen(sty) > 2 * MAXSTR - 1)
		sty[2 * MAXSTR - 1] = 0;
	sprintf(SocketPath + strlen(SocketPath), "/%s", sty);
	if ((s = MakeClientSocket(1)) == -1)
		exit(1);
	memset((char *)&m, 0, sizeof(m));
	m.type = MSG_CREATE;
	strncpy(m.m_tty, attach_tty, sizeof(m.m_tty) - 1);
	p = m.m.create.line;
	n = 0;
	if (nwin->args != nwin_undef.args)
		for (av = nwin->args; *av && n < MAXARGS - 1; ++av, ++n) {
			len = strlen(*av) + 1;
			if (p + len >= m.m.create.line + sizeof(m.m.create.line) - 1)
				break;
			strcpy(p, *av);
			p += len;
		}
	if (nwin->aka != nwin_undef.aka && p + strlen(nwin->aka) + 1 < m.m.create.line + sizeof(m.m.create.line))
		strcpy(p, nwin->aka);
	else
		*p = '\0';
	m.m.create.nargs = n;
	m.m.create.aflag = nwin->aflag;
	m.m.create.flowflag = nwin->flowflag;
	m.m.create.lflag = nwin->lflag;
	m.m.create.hheight = nwin->histheight;
	if (getcwd(m.m.create.dir, sizeof(m.m.create.dir)) == 0) {
		Msg(errno, "getcwd");
		return;
	}
	if (nwin->term != nwin_undef.term)
		strncpy(m.m.create.screenterm, nwin->term, 19);
	m.protocol_revision = MSG_REVISION;
	if (write(s, (char *)&m, sizeof m) != sizeof m)
		Msg(errno, "write");
	close(s);
}

int SendErrorMsg(char *tty, char *buf)
{
	int s;
	struct msg m;

	strncpy(m.m.message, buf, sizeof(m.m.message) - 1);
	s = MakeClientSocket(0);
	if (s < 0)
		return -1;
	m.type = MSG_ERROR;
	strncpy(m.m_tty, tty, sizeof(m.m_tty) - 1);
	m.protocol_revision = MSG_REVISION;
	(void)write(s, (char *)&m, sizeof m);
	close(s);
	return 0;
}

static void ExecCreate(struct msg *mp)
{
	struct NewWindow nwin;
	char *args[MAXARGS];
	register int n;
	register char **pp = args, *p = mp->m.create.line;
	char buf[20];

	nwin = nwin_undef;
	n = mp->m.create.nargs;
	if (n > MAXARGS - 1)
		n = MAXARGS - 1;
	/* ugly hack alert... should be done by the frontend! */
	if (n) {
		int l, num;

		l = strlen(p);
		if (IsNumColon(p, 10, buf, sizeof(buf))) {
			if (*buf)
				nwin.aka = buf;
			num = atoi(p);
			if (num < 0 || num > maxwin - 1)
				num = 0;
			nwin.StartAt = num;
			p += l + 1;
			n--;
		}
	}
	for (; n > 0; n--) {
		*pp++ = p;
		p += strlen(p) + 1;
	}
	*pp = 0;
	if (*p)
		nwin.aka = p;
	if (*args)
		nwin.args = args;
	nwin.aflag = mp->m.create.aflag;
	nwin.flowflag = mp->m.create.flowflag;
	if (*mp->m.create.dir)
		nwin.dir = mp->m.create.dir;
	nwin.lflag = mp->m.create.lflag;
	nwin.histheight = mp->m.create.hheight;
	if (*mp->m.create.screenterm)
		nwin.term = mp->m.create.screenterm;
	MakeWindow(&nwin);
}

static int CheckPid(int pid)
{
	if (pid < 2)
		return -1;
	return kill(pid, 0);
}

static int CreateTempDisplay(struct msg *m, int recvfd, Window *win)
{
	int pid;
	int attach;
	char *user;
	int i;
	struct mode Mode;
	Display *olddisplays = displays;

	switch (m->type) {
	case MSG_CONT:
	case MSG_ATTACH:
		pid = m->m.attach.apid;
		user = m->m.attach.auser;
		attach = 1;
		break;
	case MSG_DETACH:
	case MSG_POW_DETACH:
		pid = m->m.detach.dpid;
		user = m->m.detach.duser;
		attach = 0;
		break;
	default:
		return -1;
	}

	if (CheckPid(pid)) {
		Msg(0, "Attach attempt with bad pid(%d)!", pid);
		return -1;
	}
	if (recvfd != -1) {
		char *myttyname;
		i = recvfd;
		myttyname = ttyname(i);
		if (myttyname == 0 || strcmp(myttyname, m->m_tty)) {
			Msg(errno, "Attach: passed fd does not match tty: %s - %s!", m->m_tty,
			    myttyname ? myttyname : "NULL");
			close(i);
			Kill(pid, SIG_BYE);
			return -1;
		}
	} else if ((i = open(m->m_tty, O_RDWR | O_NONBLOCK, 0)) < 0) {
		Msg(errno, "Attach: Could not open %s!", m->m_tty);
		Kill(pid, SIG_BYE);
		return -1;
	}

	if (attach) {
		if (display || win) {
			write(i, "Attaching from inside of screen?\n", 33);
			close(i);
			Kill(pid, SIG_BYE);
			Msg(0, "Attach msg ignored: coming from inside.");
			return -1;
		}

	}

	/* create new display */
	GetTTY(i, &Mode);
	if (MakeDisplay(m->m_tty, attach ? m->m.attach.envterm : "", i, pid, &Mode) == 0) {
		write(i, "Could not make display.\n", 24);
		close(i);
		Msg(0, "Attach: could not make display for user %s", user);
		Kill(pid, SIG_BYE);
		return -1;
	}
	if (attach) {
		D_encoding = m->m.attach.encoding == 1 ? UTF8 : m->m.attach.encoding ? m->m.attach.encoding - 1 : 0;
		if (D_encoding < 0 || !EncodingName(D_encoding))
			D_encoding = 0;
	}

	if (iflag && olddisplays) {
		iflag = false;
#if defined(TERMIO) || defined(POSIX)
		olddisplays->d_NewMode.tio.c_cc[VINTR] = VDISABLE;
		olddisplays->d_NewMode.tio.c_lflag &= ~ISIG;
#else				/* TERMIO || POSIX */
		olddisplays->d_NewMode.m_tchars.t_intrc = -1;
#endif				/* TERMIO || POSIX */
		SetTTY(olddisplays->d_userfd, &olddisplays->d_NewMode);
	}
	SetMode(&D_OldMode, &D_NewMode, D_flow, iflag);
	SetTTY(D_userfd, &D_NewMode);
	if (fcntl(D_userfd, F_SETFL, FNBLOCK))
		Msg(errno, "Warning: NBLOCK fcntl failed");
	return 0;
}

void ReceiveMsg()
{
	int left, len;
	static struct msg m;
	char *p;
	int ns = ServerSocket;
	Window *wi;
	int recvfd = -1;

	struct sockaddr_un a;
	struct msghdr msg;
	struct iovec iov;
	char control[1024];

	len = sizeof(a);
	if ((ns = accept(ns, (struct sockaddr *)&a, (socklen_t *) & len)) < 0) {
		Msg(errno, "accept");
		return;
	}

	p = (char *)&m;
	left = sizeof(m);
	memset(&msg, 0, sizeof(msg));
	iov.iov_base = &m;
	iov.iov_len = left;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_controllen = sizeof(control);
	msg.msg_control = &control;
	while (left > 0) {
		len = recvmsg(ns, &msg, 0);
		if (len < 0 && errno == EINTR)
			continue;
		if (len < 0) {
			close(ns);
			Msg(errno, "read");
			return;
		}
		if (msg.msg_controllen) {
			struct cmsghdr *cmsg;
			for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
				size_t cl;
				char *cp;
				if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
					continue;
				cp = (char *)CMSG_DATA(cmsg);
				cl = cmsg->cmsg_len;
				while (cl >= CMSG_LEN(sizeof(int))) {
					int passedfd;
					memmove(&passedfd, cp, sizeof(int));
					if (recvfd >= 0 && passedfd != recvfd)
						close(recvfd);
					recvfd = passedfd;
					cl -= CMSG_LEN(sizeof(int));
				}
			}
		}
		p += len;
		left -= len;
		break;
	}

	while (left > 0) {
		len = read(ns, p, left);
		if (len < 0 && errno == EINTR)
			continue;
		if (len <= 0)
			break;
		p += len;
		left -= len;
	}

	close(ns);

	if (len < 0) {
		Msg(errno, "read");
		if (recvfd != -1)
			close(recvfd);
		return;
	}
	if (left > 0) {
		if (left != sizeof(m))
			Msg(0, "Message %d of %d bytes too small", left, (int)sizeof(m));
		else
		return;
	}
	if (m.protocol_revision != MSG_REVISION) {
		if (recvfd != -1)
			close(recvfd);
		Msg(0, "Invalid message (magic 0x%08x).", m.protocol_revision);
		return;
	}

	if (m.type != MSG_ATTACH && recvfd != -1) {
		close(recvfd);
		recvfd = -1;
	}

	for (display = displays; display; display = display->d_next)
		if (strcmp(D_usertty, m.m_tty) == 0)
			break;
	wi = 0;
	if (!display) {
		for (wi = windows; wi; wi = wi->w_next)
			if (!strcmp(m.m_tty, wi->w_tty)) {
				/* XXX: hmmm, rework this? */
				display = wi->w_layer.l_cvlist ? wi->w_layer.l_cvlist->c_display : 0;
				break;
			}
	}

	/* Remove the status to prevent garbage on the screen */
	if (display && D_status)
		RemoveStatus();

	if (display && !D_tcinited && m.type != MSG_HANGUP) {
		if (recvfd != -1)
			close(recvfd);
		return;		/* ignore messages for bad displays */
	}

	switch (m.type) {
	case MSG_WINCH:
		if (display)
			CheckScreenSize(1);	/* Change fore */
		break;
	case MSG_CREATE:
		/*
		 * the window that issued the create message need not be an active
		 * window. Then we create the window without having a display.
		 * Resulting in another inactive window.
		 */
		ExecCreate(&m);
		break;
	case MSG_CONT:
		if (display && D_userpid != 0 && kill(D_userpid, 0) == 0)
			break;	/* Intruder Alert */
		/* FALLTHROUGH */

	case MSG_ATTACH:
		if (CreateTempDisplay(&m, recvfd, wi))
			break;
		FinishAttach(&m);
		break;
	case MSG_ERROR:
		Msg(0, "%s", m.m.message);
		break;
	case MSG_HANGUP:
		if (!wi)	/* ignore hangups from inside */
			Hangup();
		break;
	case MSG_DETACH:
	case MSG_POW_DETACH:
		FinishDetach(&m);
		break;
	case MSG_QUERY:
		{
			char *oldSocketPath = SaveStr(SocketPath);
			strncpy(SocketPath, m.m.command.writeback, MAXPATHLEN + 2 * MAXSTR);
			int s = MakeClientSocket(0);
			strncpy(SocketPath, oldSocketPath, MAXPATHLEN + 2 * MAXSTR);
			Free(oldSocketPath);
			if (s >= 0) {
				queryflag = s;
				DoCommandMsg(&m);
				close(s);
			} else
				queryflag = -1;

			Kill(m.m.command.apid, (queryflag >= 0) ? SIGCONT : SIG_BYE);	/* Send SIG_BYE if an error happened */
			queryflag = -1;
		}
		break;
	case MSG_COMMAND:
		DoCommandMsg(&m);
		break;
	default:
		Msg(0, "Invalid message (type %d).", m.type);
	}
}

void ReceiveRaw(int s)
{
	char rd[256];
	int len = 0;
	struct sockaddr_un a;
	len = sizeof(a);
	if ((s = accept(s, (struct sockaddr *)&a, (socklen_t *) & len)) < 0) {
		Msg(errno, "accept");
		return;
	}
	while ((len = read(s, rd, 255)) > 0) {
		rd[len] = 0;
		printf("%s", rd);
	}
	close(s);
}

/*
 * Set the mode bits of the socket to the current status
 */
int chsock()
{
	int r;
	uid_t euid = geteuid();
	if (euid != real_uid) {
		if (UserContext() <= 0)
			return UserStatus();
	}
	r = chmod(SocketPath, SOCKMODE);
	/*
	 * Sockets usually reside in the /tmp/ area, where sysadmin scripts
	 * may be happy to remove old files. We manually prevent the socket
	 * from becoming old. (chmod does not touch mtime).
	 */
	(void)utimes(SocketPath, NULL);

	if (euid != real_uid)
		UserReturn(r);
	return r;
}

/*
 * Try to recreate the socket/pipe
 */
int RecoverSocket()
{
	close(ServerSocket);
	if (geteuid() != real_uid) {
		if (UserContext() > 0)
			UserReturn(unlink(SocketPath));
		(void)UserStatus();
	} else
		(void)unlink(SocketPath);

	if ((ServerSocket = MakeServerSocket()) < 0)
		return 0;
	evdeq(&serv_read);
	serv_read.fd = ServerSocket;
	evenq(&serv_read);
	return 1;
}

static void FinishAttach(struct msg *m)
{
	char *p;
	int pid;
	int noshowwin;
	Window *wi;

	struct sigaction sigact;

	pid = D_userpid;

	if (m->m.attach.detachfirst == MSG_DETACH || m->m.attach.detachfirst == MSG_POW_DETACH)
		FinishDetach(m);

	/*
	 * We reboot our Terminal Emulator. Forget all we knew about
	 * the old terminal, reread the termcap entries in .screenrc
	 * (and nothing more from .screenrc is read. Mainly because
	 * I did not check, weather a full reinit is safe. jw)
	 * and /etc/screenrc, and initialise anew.
	 */
	if (extra_outcap)
		free(extra_outcap);
	if (extra_incap)
		free(extra_incap);
	extra_incap = extra_outcap = 0;
#ifdef ETCSCREENRC
#ifdef ALLOW_SYSSCREENRC
	if ((p = getenv("SYSSCREENRC")))
		StartRc(p, 1);
	else
#endif
		StartRc(ETCSCREENRC, 1);
#endif
	StartRc(RcFileName, 1);
	if (InitTermcap(m->m.attach.columns, m->m.attach.lines)) {
		FreeDisplay();
		Kill(pid, SIG_BYE);
		return;
	}
	MakeDefaultCanvas();
	InitTerm(m->m.attach.adaptflag);	/* write init string on fd */
	if (displays->d_next == 0)
		(void)chsock();
	sigemptyset (&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = SigHup;
	sigaction(SIGHUP, &sigact, NULL);
	if (m->m.attach.esc != -1 && m->m.attach.meta_esc != -1) {
		DefaultEsc = m->m.attach.esc;
		DefaultMetaEsc = m->m.attach.meta_esc;
	}
#ifdef UTMPOK
	/*
	 * we set the Utmp slots again, if we were detached normally
	 * and if we were detached by ^Z.
	 * don't log zomies back in!
	 */
	RemoveLoginSlot();
	if (displays->d_next == 0)
		for (wi = windows; wi; wi = wi->w_next)
			if (wi->w_ptyfd >= 0 && wi->w_slot != (slot_t) - 1)
				SetUtmp(wi);
#endif

	D_fore = NULL;
	if (layout_attach) {
		Layout *lay = layout_attach;
		if (lay == &layout_last_marker)
			lay = layout_last;
		if (lay) {
			LoadLayout(lay);
			SetCanvasWindow(D_forecv, 0);
		}
	}
	/*
	 * there may be a window that we remember from last detach:
	 */
	if (DetachWin >= 0)
		fore = wtab[DetachWin];
	else
		fore = 0;

	/* Wayne wants us to restore the other window too. */
	if (DetachWinOther >= 0)
		D_other = wtab[DetachWinOther];

	noshowwin = 0;
	if (*m->m.attach.preselect) {
		if (!strcmp(m->m.attach.preselect, "="))
			fore = 0;
		else if (!strcmp(m->m.attach.preselect, "-")) {
			fore = 0;
			noshowwin = 1;
		} else if (!strcmp(m->m.attach.preselect, "+")) {
			struct action newscreen;
			char *na = 0;
			newscreen.nr = RC_SCREEN;
			newscreen.args = &na;
			newscreen.quiet = 0;
			DoAction(&newscreen, -1);
		} else
			fore = FindNiceWindow(fore, m->m.attach.preselect);
	} else
		fore = FindNiceWindow(fore, 0);
	if (fore)
		SetForeWindow(fore);
	else if (!noshowwin) {
		Display *olddisplay = display;
		flayer = D_forecv->c_layer;
		display_windows(1, WLIST_NUM, (Window *)0);
		noshowwin = 1;
		display = olddisplay;	/* display_windows can change display */
	}
	Activate(0);
	ResetIdle();
	if (!D_fore && !noshowwin)
		ShowWindows(-1);
	if (displays->d_next == 0 && console_window) {
		if (TtyGrabConsole(console_window->w_ptyfd, 1, "reattach") == 0)
			Msg(0, "console %s is on window %d", HostName, console_window->w_number);
	}
}

static void FinishDetach(struct msg *m)
{
	Display *next, **d, *det;
	int pid;

	if (m->type == MSG_ATTACH)
		pid = D_userpid;
	else
		pid = m->m.detach.dpid;

	/* Remove the temporary display prompting for the password from the list */
	for (d = &displays; (det = *d); d = &det->d_next) {
		if (det->d_userpid == pid)
			break;
	}
	if (det) {
		*d = det->d_next;
		det->d_next = 0;
	}

	for (display = displays; display; display = next) {
		next = display->d_next;
		if (m->type == MSG_POW_DETACH)
			Detach(D_REMOTE_POWER);
		else if (m->type == MSG_DETACH)
			Detach(D_REMOTE);
		else if (m->type == MSG_ATTACH) {
			if (m->m.attach.detachfirst == MSG_POW_DETACH)
				Detach(D_REMOTE_POWER);
			else if (m->m.attach.detachfirst == MSG_DETACH)
				Detach(D_REMOTE);
		}
	}
	display = displays = det;
	if (m->type != MSG_ATTACH) {
		if (display)
			FreeDisplay();
		Kill(pid, SIGCONT);
	}
}

/* 'end' is exclusive, i.e. you should *not* write in *end */
static char *strncpy_escape_quote(char *dst, const char *src, const char *end)
{
	while (*src && dst < end) {
		if (*src == '"') {
			if (dst + 2 < end)	/* \\ \" \0 */
				*dst++ = '\\';
			else
				return NULL;
		}
		*dst++ = *src++;
	}
	if (dst >= end)
		return NULL;

	*dst = '\0';
	return dst;
}

static void DoCommandMsg(struct msg *mp)
{
	char *args[MAXARGS];
	int argl[MAXARGS];
	char fullcmd[MAXSTR];
	register char *fc;
	int n;
	register char *p = mp->m.command.cmd;

	n = mp->m.command.nargs;
	if (n > MAXARGS - 1)
		n = MAXARGS - 1;
	for (fc = fullcmd; n > 0; n--) {
		int len = strlen(p);
		*fc++ = '"';
		if (!(fc = strncpy_escape_quote(fc, p, fullcmd + sizeof(fullcmd) - 2))) {	/* '"' ' ' */
			Msg(0, "Remote command too long.");
			queryflag = -1;
			return;
		}
		p += len + 1;
		*fc++ = '"';
		*fc++ = ' ';
	}
	if (fc != fullcmd)
		*--fc = 0;
	if (Parse(fullcmd, sizeof fullcmd, args, argl) <= 0) {
		queryflag = -1;
		return;
	}
	for (fore = windows; fore; fore = fore->w_next)
		if (!strcmp(mp->m_tty, fore->w_tty)) {
			if (!display)
				display = fore->w_layer.l_cvlist ? fore->w_layer.l_cvlist->c_display : 0;

			/* If the window is not visibile in any display, then do not use the originating window as
			 * the foreground window for the command. This way, if there is an existing display, then
			 * the command will execute from the foreground window of that display. This is necessary so
			 * that commands that are relative to the window (e.g. 'next' etc.) do the right thing. */
			if (!fore->w_layer.l_cvlist || !fore->w_layer.l_cvlist->c_display)
				fore = NULL;
			break;
		}
	if (!display)
		display = displays;	/* sigh */
	if (*mp->m.command.preselect) {
		int i = -1;
		if (strcmp(mp->m.command.preselect, "-")) {
			i = WindowByNoN(mp->m.command.preselect);
			if (i < 0 || !wtab[i]) {
				Msg(0, "Could not find pre-select window.");
				queryflag = -1;
				return;
			}
		}
		fore = i >= 0 ? wtab[i] : 0;
	} else if (!fore) {
		if (display)
			fore = Layer2Window(display->d_forecv->c_layer);
		if (!fore) {
			fore = DetachWin >= 0 ? wtab[DetachWin] : 0;
			fore = FindNiceWindow(fore, 0);
		}
	}
	if (!fore)
		fore = windows;	/* sigh */
	if (*args) {
		char *oldrcname = rc_name;
		rc_name = "-X";
		flayer = fore ? &fore->w_layer : 0;
		if (fore && fore->w_savelayer && (fore->w_blocked || fore->w_savelayer->l_cvlist == 0))
			flayer = fore->w_savelayer;
		DoCommand(args, argl);
		rc_name = oldrcname;
	}
}

int SendAttachMsg(int s, struct msg *m, int fd)
{
	int r;
	struct msghdr msg;
	struct iovec iov;
	char buf[CMSG_SPACE(sizeof(int))];
	struct cmsghdr *cmsg;

	iov.iov_base = (char *)m;
	iov.iov_len = sizeof(*m);
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	memmove(CMSG_DATA(cmsg), &fd, sizeof(int));
	msg.msg_controllen = cmsg->cmsg_len;
	while (1) {
		r = sendmsg(s, &msg, 0);
		if (r == -1 && errno == EINTR)
			continue;
		if (r == -1)
			return -1;
		return 0;
	}
}
