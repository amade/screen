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

#include "screen.h"

#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <locale.h>
#if defined(HAVE_NL_LANGINFO)
#include <langinfo.h>
#endif

#include "winmsg.h"

#include "version.h"
#include "logfile.h"		/* islogfile, logfflush */
#include "fileio.h"
#include "mark.h"
#include "utmp.h"

extern char **environ;

int force_vt = 1;
int VBellWait, MsgWait, MsgMinWait, SilenceWait;

char *ShellProg;
char *ShellArgs[2];

struct backtick;

static struct passwd *getpwbyname(char *, struct passwd *);
static void SigChldHandler(void);
static void SigChld(int);
static void SigInt(int);
static void CoreDump(int);
static void FinitHandler(int);
static void DoWait(void);
static void serv_read_fn(Event *, void *);
static void serv_select_fn(Event *, void *);
static void logflush_fn(Event *, void *);
static int IsSymbol(char *, char *);
static char *ParseChar(char *, char *);
static int ParseEscape(char *);

int nversion;			/* numerical version, used for secondary DA */

/* the attacher */
struct passwd *ppp;
char *attach_tty;
int attach_fd = -1;
char *attach_term;
char *LoginName;
struct mode attach_Mode;

char SocketPath[MAXPATHLEN + 2 * MAXSTR];
char *SocketName;			/* SocketName is pointer in SocketPath */
char *SocketMatch = NULL;		/* session id command line argument */
int ServerSocket = -1;
Event serv_read;
Event serv_select;
Event logflushev;

char **NewEnv = NULL;

char *RcFileName = NULL;
char *home;

char *screenlogfile;		/* filename layout */
int log_flush = 10;		/* flush interval in seconds */
bool logtstamp_on = false;	/* tstamp disabled */
char *logtstamp_string;		/* stamp layout */
int logtstamp_after = 120;	/* first tstamp after 120s */
char *hardcopydir = NULL;
char *BellString;
char *VisualBellString;
char *ActivityString;
char *BufferFile;
char *PowDetachString;
char *hstatusstring;
char *captionstring;
char *timestring;
char *wliststr;
char *wlisttit;
bool auto_detach = true;
bool adaptflag, cmdflag, iflag, lsflag, quietflag, wipeflag, xflag;
int rflag, dflag;
int queryflag = -1;
bool hastruecolor = false;

char *multi;
char *multi_home;
unsigned int multi_uid;
int own_uid;
int multiattach;
int tty_mode;
int tty_oldmode = -1;

char HostName[MAXSTR];
int MasterPid, PanicPid;
uid_t real_uid, eff_uid;
gid_t real_gid, eff_gid;
bool default_startup;
int ZombieKey_destroy, ZombieKey_resurrect, ZombieKey_onerror;
char *preselect = NULL;		/* only used in Attach() */

char *screenencodings;

bool cjkwidth;

uint16_t maxwin;

Layer *flayer;
Window *fore;
Window *windows;
Window *console_window;

#ifdef ENABLE_TELNET
int af;
#endif

/*
 * Do this last
 */
#include "attacher.h"
#include "encoding.h"
#include "help.h"
#include "misc.h"
#include "process.h"
#include "socket.h"
#include "termcap.h"
#include "tty.h"

char strnomem[] = "Out of memory.";

static int InterruptPlease;
static int GotSigChld;

/********************************************************************/
/********************************************************************/
/********************************************************************/

static struct passwd *getpwbyname(char *name, struct passwd *ppp)
{
	int n;

	if (!ppp && !(ppp = getpwnam(name)))
		return NULL;

	/* Do password sanity check..., allow ##user for SUN_C2 security */
	n = 0;
	if (ppp->pw_passwd[0] == '#' && ppp->pw_passwd[1] == '#' && strcmp(ppp->pw_passwd + 2, ppp->pw_name) == 0)
		n = 13;
	for (; n < 13; n++) {
		char c = ppp->pw_passwd[n];
		if (!(c == '.' || c == '/' || c == '$' ||
		      (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
			break;
	}

	if (n < 13)
		ppp->pw_passwd = 0;
	if (ppp->pw_passwd && strlen(ppp->pw_passwd) == 13 + 11)
		ppp->pw_passwd[13] = 0;	/* beware of linux's long passwords */

	return ppp;
}

static char *locale_name(void)
{
	static char *s;

	s = getenv("LC_ALL");
	if (s == NULL)
		s = getenv("LC_CTYPE");
	if (s == NULL)
		s = getenv("LANG");
	if (s == NULL)
		s = "C";
	return s;
}

int main(int argc, char **argv)
{
	int n;
	char *ap;
	char *av0;
	char socknamebuf[FILENAME_MAX + 1];
	int mflag = 0;
	char *myname = (argc == 0) ? "screen" : argv[0];
	char *SocketDir;
	struct stat st;
	mode_t oumask;
	struct NewWindow nwin;
	bool detached = false;	/* start up detached */
	char *sockp;
	char *sty = 0;

	/*
	 *  First, close all unused descriptors
	 *  (otherwise, we might have problems with the select() call)
	 */
	closeallfiles(0);
	snprintf(version, 59, "%d.%d.%d-%s (build on %s %s) ", REV, VERS, PATCHLEVEL, STATE, __DATE__, __TIME__);
	nversion = REV * 10000 + VERS * 100 + PATCHLEVEL;

	BellString = SaveStr("Bell in window %n");
	VisualBellString = SaveStr("   Wuff,  Wuff!!  ");
	ActivityString = SaveStr("Activity in window %n");
	screenlogfile = SaveStr("screenlog.%n");
	logtstamp_string = SaveStr("-- %n:%t -- time-stamp -- %M/%d/%y %c:%s --\n");
	hstatusstring = SaveStr("%h");
	captionstring = SaveStr("%4n %t");
	timestring = SaveStr("%c:%s %M %d %H%? %l%?");
	wlisttit = SaveStr(" Num Name%=Flags");
	wliststr = SaveStr("%4n %t%=%f");
	BufferFile = SaveStr(DEFAULT_BUFFERFILE);
	ShellProg = NULL;
	PowDetachString = 0;
	default_startup = (argc > 1) ? false : true;
	adaptflag = false;
	VBellWait = VBELLWAIT * 1000;
	MsgWait = MSGWAIT * 1000;
	MsgMinWait = MSGMINWAIT * 1000;
	SilenceWait = SILENCEWAIT;
	zmodem_sendcmd = SaveStr("!!! sz -vv -b ");
	zmodem_recvcmd = SaveStr("!!! rz -vv -b -E");

	CompileKeys((char *)0, 0, mark_key_tab);
	InitBuiltinTabs();
	screenencodings = SaveStr(SCREENENCODINGS);
	cjkwidth = 0;
	nwin = nwin_undef;
	nwin_options = nwin_undef;
	strncpy(screenterm, "screen", MAXTERMLEN);
	screenterm[MAXTERMLEN] = '\0';
#ifdef ENABLE_TELNET
	af = AF_UNSPEC;
#endif

	av0 = *argv;
	/* if this is a login screen, assume -RR */
	if (*av0 == '-') {
		rflag = 4;
		xflag = true;
		ShellProg = SaveStr(DefaultShell);	/* to prevent nasty circles */
	}
	while (argc > 0) {
		ap = *++argv;
		if (--argc > 0 && *ap == '-') {
			if (ap[1] == '-' && ap[2] == 0) {
				argv++;
				argc--;
				break;
			}
			if (ap[1] == '-' && !strncmp(ap, "--version", 9))
				Panic(0, "Screen version %s", version);
			if (ap[1] == '-' && !strncmp(ap, "--help", 6))
				exit_with_usage(myname, NULL, NULL);
			while (ap && *ap && *++ap) {
				switch (*ap) {
#ifdef ENABLE_TELNET
				case '4':
					af = AF_INET;
					break;
				case '6':
					af = AF_INET6;
					break;
#endif
				case 'a':
					nwin_options.aflag = true;
					break;
				case 'A':
					adaptflag = true;
					break;
				case 'p':	/* preselect */
					if (*++ap)
						preselect = ap;
					else {
						if (!--argc)
							exit_with_usage(myname, "Specify a window to preselect with -p",
									NULL);
						preselect = *++argv;
					}
					ap = NULL;
					break;
				case 'c':
					if (*++ap)
						RcFileName = ap;
					else {
						if (--argc == 0)
							exit_with_usage(myname,
									"Specify an alternate rc-filename with -c",
									NULL);
						RcFileName = *++argv;
					}
					ap = NULL;
					break;
				case 'e':
					if (!*++ap) {
						if (--argc == 0)
							exit_with_usage(myname, "Specify command characters with -e",
									NULL);
						ap = *++argv;
					}
					if (ParseEscape(ap))
						Panic(0, "Two characters are required with -e option, not '%s'.", ap);
					ap = NULL;
					break;
				case 'f':
					ap++;
					switch (*ap++) {
					case 'n':
					case '0':
						nwin_options.flowflag = FLOW_ON;
						break;
					case '\0':
						ap--;
						/* FALLTHROUGH */
					case 'y':
					case '1':
						nwin_options.flowflag = FLOW_OFF;
						break;
					case 'a':
						nwin_options.flowflag = FLOW_AUTOFLAG;
						break;
					default:
						exit_with_usage(myname, "Unknown flow option -%s", --ap);
					}
					break;
				case 'h':
					if (--argc == 0)
						exit_with_usage(myname, NULL, NULL);
					nwin_options.histheight = atoi(*++argv);
					if (nwin_options.histheight < 0)
						exit_with_usage(myname, "-h: %s: negative scrollback size?", *argv);
					break;
				case 'i':
					iflag = true;
					break;
				case 't':	/* title, the former AkA == -k */
					if (--argc == 0)
						exit_with_usage(myname, "Specify a new window-name with -t", NULL);
					nwin_options.aka = *++argv;
					break;
				case 'l':
					ap++;
					switch (*ap++) {
					case 'n':
					case '0':
						nwin_options.lflag = 0;
						break;
					case '\0':
						ap--;
						/* FALLTHROUGH */
					case 'y':
					case '1':
						nwin_options.lflag = 1;
						break;
					case 'a':
						nwin_options.lflag = 3;
						break;
					case 's':	/* -ls */
					case 'i':	/* -list */
						lsflag = true;
						if (argc > 1 && !SocketMatch) {
							SocketMatch = *++argv;
							argc--;
						}
						ap = NULL;
						break;
					default:
						exit_with_usage(myname, "%s: Unknown suboption to -l", --ap);
					}
					break;
				case 'w':
					if (strcmp(ap + 1, "ipe"))
						exit_with_usage(myname, "Unknown option %s", --ap);
					lsflag = true;
					wipeflag = true;
					if (argc > 1 && !SocketMatch) {
						SocketMatch = *++argv;
						argc--;
					}
					break;
				case 'L':
					nwin_options.Lflag = true;
					break;
				case 'm':
					mflag = 1;
					break;
				case 'O':	/* to be (or not to be?) deleted. jw. */
					force_vt = 0;
					break;
				case 'T':
					if (--argc == 0)
						exit_with_usage(myname, "Specify terminal-type with -T", NULL);
					if (strlen(*++argv) < MAXTERMLEN) {
						strncpy(screenterm, *argv, MAXTERMLEN);
						screenterm[MAXTERMLEN] = '\0';
					} else
						Panic(0, "-T: terminal name too long. (max. %d char)", MAXTERMLEN);
					nwin_options.term = screenterm;
					break;
				case 'q':
					quietflag = true;
					break;
				case 'Q':
					queryflag = 1;
					cmdflag = true;
					break;
				case 'r':
				case 'R':
				case 'x':
					if (argc > 1 && *argv[1] != '-' && !SocketMatch) {
						SocketMatch = *++argv;
						argc--;
					}
					if (*ap == 'x')
						xflag = true;
					if (rflag)
						rflag = 2;
					rflag += (*ap == 'R') ? 2 : 1;
					break;
				case 'd':
					dflag = 1;
					/* FALLTHROUGH */
				case 'D':
					if (!dflag)
						dflag = 2;
					if (argc == 2) {
						if (*argv[1] != '-' && !SocketMatch) {
							SocketMatch = *++argv;
							argc--;
						}
					}
					break;
				case 's':
					if (--argc == 0)
						exit_with_usage(myname, "Specify shell with -s", NULL);
					if (ShellProg)
						free(ShellProg);
					ShellProg = SaveStr(*++argv);
					break;
				case 'S':
					if (!SocketMatch) {
						if (--argc == 0)
							exit_with_usage(myname, "Specify session-name with -S", NULL);
						SocketMatch = *++argv;
					}
					if (!*SocketMatch)
						exit_with_usage(myname, "Empty session-name?", NULL);
					break;
				case 'X':
					cmdflag = true;
					break;
				case 'v':
					Panic(0, "Screen version %s", version);
					/* NOTREACHED */
				case 'U':
					nwin_options.encoding = nwin_options.encoding == -1 ? UTF8 : 0;
					break;
				default:
					exit_with_usage(myname, "Unknown option %s", --ap);
				}
			}
		} else
			break;
	}

	real_uid = getuid();
	real_gid = getgid();
	eff_uid = geteuid();
	eff_gid = getegid();

	xsignal(SIGSEGV, CoreDump);

	setlocale(LC_ALL, "");
	if (nwin_options.encoding == -1) {
		/* ask locale if we should start in UTF-8 mode */
#ifdef HAVE_NL_LANGINFO
		nwin_options.encoding = FindEncoding(nl_langinfo(CODESET));
#else
		char *s;
		if ((s = locale_name()) && InStr(s, "UTF-8"))
			nwin_options.encoding = UTF8;
#endif
	}
	{
		char *s;
		if ((s = locale_name())) {
			if (!strncmp(s, "zh_", 3) || !strncmp(s, "ja_", 3) || !strncmp(s, "ko_", 3)) {
				cjkwidth = 1;
			}
		}
	}
	if (nwin_options.aka) {
		if (nwin_options.encoding > 0) {
			size_t len = strlen(nwin_options.aka);
			size_t newsz;
			char *newbuf = malloc(3 * len);
			if (!newbuf)
				Panic(0, "%s", strnomem);
			newsz = RecodeBuf((unsigned char *)nwin_options.aka, len,
					  nwin_options.encoding, 0, (unsigned char *)newbuf);
			newbuf[newsz] = '\0';
			nwin_options.aka = newbuf;
		} else {
			/* If we just use the original value from av,
			   subsequent shelltitle invocations will attempt to free
			   space we don't own... */
			nwin_options.aka = SaveStr(nwin_options.aka);
		}
	}

	if (SocketMatch && strlen(SocketMatch) >= MAXSTR)
		Panic(0, "Ridiculously long socketname - try again.");
	if (cmdflag && !rflag && !dflag && !xflag)
		xflag = true;
	if (!cmdflag && dflag && mflag && !(rflag || xflag))
		detached = true;
	nwin = nwin_options;
	nwin.encoding = nwin_undef.encoding;	/* let screenrc overwrite it */
	if (argc)
		nwin.args = argv;


	if (!ShellProg) {
		char *sh;

		sh = getenv("SHELL");
		ShellProg = SaveStr(sh ? sh : DefaultShell);
	}
	ShellArgs[0] = ShellProg;
	home = getenv("HOME");
	if (!mflag && !SocketMatch) {
		sty = getenv("STY");
		if (sty && *sty == 0)
			sty = 0;
	}

	own_uid = multi_uid = real_uid;
	if (SocketMatch && (sockp = strchr(SocketMatch, '/'))) {
		*sockp = 0;
		multi = SocketMatch;
		SocketMatch = sockp + 1;
		if (*multi) {
			struct passwd *mppp;
			if ((mppp = getpwnam(multi)) == (struct passwd *)0)
				Panic(0, "Cannot identify account '%s'.", multi);
			multi_uid = mppp->pw_uid;
			multi_home = SaveStr(mppp->pw_dir);
			if (strlen(multi_home) > MAXPATHLEN - 10)
				Panic(0, "home directory path too long");
			/* always fake multi attach mode */
			if (rflag || lsflag)
				xflag = 1;
			detached = false;
			multiattach = 1;
		}
		/* Special case: effective user is multiuser. */
		if (eff_uid && (multi_uid != eff_uid))
			Panic(0, "Must run suid root for multiuser support.");
	}
	if (SocketMatch && *SocketMatch == 0)
		SocketMatch = 0;

	if ((LoginName = getlogin()) != NULL) {
		if ((ppp = getpwnam(LoginName)) != (struct passwd *)0)
			if (ppp->pw_uid != real_uid)
				ppp = (struct passwd *)0;
	}
	if (ppp == 0) {
		if ((ppp = getpwuid(real_uid)) == 0) {
			Panic(0, "getpwuid() can't identify your account!");
			exit(1);
		}
		LoginName = ppp->pw_name;
	}
	LoginName = SaveStr(LoginName);

	ppp = getpwbyname(LoginName, ppp);

#if !defined(SOCKDIR)
	if (multi && !multiattach) {
		if (home && strcmp(home, ppp->pw_dir))
			Panic(0, "$HOME must match passwd entry for multiuser screens.");
	}
#endif

#define SET_GUID() do \
  { \
    setgid(real_gid); \
    setuid(real_uid); \
    eff_uid = real_uid; \
    eff_gid = real_gid; \
  } while (0)

#define SET_TTYNAME(fatal) do \
  { \
    if (!(attach_tty = ttyname(0))) \
      { \
	if (fatal) \
	  Panic(0, "Must be connected to a terminal."); \
	else \
	  attach_tty = ""; \
      } \
    else \
      { \
	if (stat(attach_tty, &st)) \
	  Panic(errno, "Cannot access '%s'", attach_tty); \
	if (CheckTtyname(attach_tty)) \
	  Panic(0, "Bad tty '%s'", attach_tty); \
      } \
    if (strlen(attach_tty) >= MAXPATHLEN) \
      Panic(0, "TtyName too long - sorry."); \
  } while (0)

	if (home == 0 || *home == '\0')
		home = ppp->pw_dir;
	if (strlen(LoginName) > MAXLOGINLEN)
		Panic(0, "LoginName too long - sorry.");
	if (multi && strlen(multi) > MAXLOGINLEN)
		Panic(0, "Screen owner name too long - sorry.");
	if (strlen(home) > MAXPATHLEN)
		Panic(0, "$HOME too long - sorry.");

	attach_tty = "";
	if (!detached && !lsflag && !cmdflag && !(dflag && !mflag && !rflag && !xflag)
	    && !(sty && !SocketMatch && !mflag && !rflag && !xflag)) {
		int fl;

		/* ttyname implies isatty */
		SET_TTYNAME(1);
		tty_mode = (int)st.st_mode & 0777;

		fl = fcntl(0, F_GETFL, 0);
		if (fl != -1 && (fl & (O_RDWR | O_RDONLY | O_WRONLY)) == O_RDWR)
			attach_fd = 0;

		if (attach_fd == -1) {
			if ((n = secopen(attach_tty, O_RDWR | O_NONBLOCK, 0)) < 0)
				Panic(0, "Cannot open your terminal '%s' - please check.", attach_tty);
			close(n);
		}

		if ((attach_term = getenv("TERM")) == 0 || *attach_term == 0)
			Panic(0, "Please set a terminal type.");
		if (strlen(attach_term) > MAXTERMLEN)
			Panic(0, "$TERM too long - sorry.");
		GetTTY(0, &attach_Mode);
	}

	oumask = umask(0);	/* well, unsigned never fails? jw. */

	SocketDir = getenv("SCREENDIR");
	if (SocketDir) {
		if (strlen(SocketDir) >= MAXPATHLEN - 1)
			Panic(0, "Ridiculously long $SCREENDIR - try again.");
		if (multi)
			Panic(0, "No $SCREENDIR with multi screens, please.");
	}
	if (multiattach) {
#ifndef SOCKDIR
		sprintf(SocketPath, "%s/.screen", multi_home);
		SocketDir = SocketPath;
#else
		SocketDir = SOCKDIR;
		sprintf(SocketPath, "%s/S-%s", SocketDir, multi);
#endif
	} else {
#ifndef SOCKDIR
		if (SocketDir == 0) {
			sprintf(SocketPath, "%s/.screen", home);
			SocketDir = SocketPath;
		}
#endif
		if (SocketDir) {
			if (access(SocketDir, F_OK)) {
				if (UserContext() > 0) {
					if (mkdir(SocketDir, 0700))
						UserReturn(0);
					UserReturn(1);
				}
				if (UserStatus() <= 0)
					Panic(0, "Cannot make directory '%s'.", SocketDir);
			}
			if (SocketDir != SocketPath)
				strncpy(SocketPath, SocketDir, MAXPATHLEN + 2 * MAXSTR);
		}
#ifdef SOCKDIR
		else {
			SocketDir = SOCKDIR;
			if (stat(SocketDir, &st)) {
				n = (eff_uid == 0 && (real_uid || eff_gid == real_gid)) ? 0755 :
				    (eff_gid != real_gid) ? 0775 :
#ifdef S_ISVTX
				    0777 | S_ISVTX;
#else
				    0777;
#endif
				if (mkdir(SocketDir, n) == -1)
					Panic(errno, "Cannot make directory '%s'", SocketDir);
			} else {
				if (!S_ISDIR(st.st_mode))
					Panic(0, "'%s' must be a directory.", SocketDir);
				if (eff_uid == 0 && real_uid && st.st_uid != eff_uid)
					Panic(0, "Directory '%s' must be owned by root.", SocketDir);
				n = (eff_uid == 0 && (real_uid || (st.st_mode & 0775) != 0775)) ? 0755 :
				    (eff_gid == st.st_gid && eff_gid != real_gid) ? 0775 : 0777;
				if (((int)st.st_mode & 0777) != n)
					Panic(0, "Directory '%s' must have mode %03o.", SocketDir, n);
			}
			sprintf(SocketPath, "%s/S-%s", SocketDir, LoginName);
			if (access(SocketPath, F_OK)) {
				if (mkdir(SocketPath, 0700) == -1 && errno != EEXIST)
					Panic(errno, "Cannot make directory '%s'", SocketPath);
				(void)chown(SocketPath, real_uid, real_gid);
			}
		}
#endif
	}

	if (stat(SocketPath, &st) == -1)
		Panic(errno, "Cannot access %s", SocketPath);
	else if (!S_ISDIR(st.st_mode))
		Panic(0, "%s is not a directory.", SocketPath);
	if (multi) {
		if (st.st_uid != multi_uid)
			Panic(0, "%s is not the owner of %s.", multi, SocketPath);
	} else {
#ifdef SOCKDIR			/* if SOCKDIR is not defined, the socket is in $HOME.
				   in that case it does not make sense to compare uids. */
		if (st.st_uid != real_uid)
			Panic(0, "You are not the owner of %s.", SocketPath);
#endif
	}
	if ((st.st_mode & 0777) != 0700)
		Panic(0, "Directory %s must have mode 700.", SocketPath);
	if (SocketMatch && strchr(SocketMatch, '/'))
		Panic(0, "Bad session name '%s'", SocketMatch);
	SocketName = SocketPath + strlen(SocketPath) + 1;
	*SocketName = 0;
	(void)umask(oumask);

	(void)gethostname(HostName, MAXSTR);
	HostName[MAXSTR - 1] = '\0';
	if ((ap = strchr(HostName, '.')) != NULL)
		*ap = '\0';

	if (lsflag) {
		int i, fo, oth;

		if (multi)
			real_uid = multi_uid;
		SET_GUID();
		i = FindSocket((int *)NULL, &fo, &oth, SocketMatch);
		if (quietflag) {
			if (rflag)
				exit(10 + i);
			else
				exit(9 + (fo || oth ? 1 : 0) + fo);
		}
		if (fo == 0)
			Panic(0, "No Sockets found in %s.\n", SocketPath);
		Msg(0, "%d Socket%s in %s.", fo, fo > 1 ? "s" : "", SocketPath);
		eexit(0);
	}
	xsignal(SIG_BYE, AttacherFinit);	/* prevent races */
	if (cmdflag) {
		/* attach_tty is not mandatory */
		SET_TTYNAME(0);
		if (!*argv)
			Panic(0, "Please specify a command.");
		SET_GUID();
		SendCmdMessage(sty, SocketMatch, argv, queryflag >= 0);
		exit(0);
	} else if (rflag || xflag) {
		if (Attach(MSG_ATTACH)) {
			Attacher();
			/* NOTREACHED */
		}
		if (multiattach)
			Panic(0, "Can't create sessions of other users.");
	} else if (dflag && !mflag) {
		SET_TTYNAME(0);
		Attach(MSG_DETACH);
		Msg(0, "[%s %sdetached.]\n", SocketName, (dflag > 1 ? "power " : ""));
		eexit(0);
		/* NOTREACHED */
	}
	if (!SocketMatch && !mflag && sty) {
		/* attach_tty is not mandatory */
		SET_TTYNAME(0);
		SET_GUID();
		nwin_options.args = argv;
		SendCreateMsg(sty, &nwin);
		exit(0);
		/* NOTREACHED */
	}
	nwin_compose(&nwin_default, &nwin_options, &nwin_default);

	if (!detached || dflag != 2)
		MasterPid = fork();
	else
		MasterPid = 0;

	switch (MasterPid) {
	case -1:
		Panic(errno, "fork");
		/* NOTREACHED */
	case 0:
		break;
	default:
		if (detached)
			exit(0);
		if (SocketMatch)
			sprintf(socknamebuf, "%d.%s", MasterPid, SocketMatch);
		else
			sprintf(socknamebuf, "%d.%s.%s", MasterPid, stripdev(attach_tty), HostName);
		for (ap = socknamebuf; *ap; ap++)
			if (*ap == '/')
				*ap = '-';
		if (strlen(socknamebuf) > FILENAME_MAX)
			socknamebuf[FILENAME_MAX - 1] = 0;
		sprintf(SocketPath + strlen(SocketPath), "/%s", socknamebuf);
		SET_GUID();
		Attacher();
		/* NOTREACHED */
	}

	if (!detached)
		PanicPid = getppid();

	if (DefaultEsc == -1)
		DefaultEsc = Ctrl('a');
	if (DefaultMetaEsc == -1)
		DefaultMetaEsc = 'a';

	ap = av0 + strlen(av0) - 1;
	while (ap >= av0) {
		if (!strncmp("screen", ap, 6)) {
			strncpy(ap, "SCREEN", 6);	/* name this process "SCREEN-BACKEND" */
			break;
		}
		ap--;
	}
	if (ap < av0)
		*av0 = 'S';

	if (!detached) {
		if (attach_fd == -1) {
			if ((n = secopen(attach_tty, O_RDWR | O_NONBLOCK, 0)) < 0)
				Panic(0, "Cannot reopen '%s' - please check.", attach_tty);
		} else
			n = dup(attach_fd);
	} else
		n = -1;
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);

	freopen("/dev/null", "w", stderr);

	/*
	 * This guarantees that the session owner is listed, even when we
	 * start detached. From now on we should not refer to 'LoginName'
	 * any more, use users->u_name instead.
	 */
	if (UserAdd(LoginName, (char *)0, (struct acluser **)0) < 0)
		Panic(0, "Could not create user info");
	if (!detached) {
		if (MakeDisplay(LoginName, attach_tty, attach_term, n, getppid(), &attach_Mode) == 0)
			Panic(0, "Could not alloc display");
		PanicPid = 0;
		D_encoding = nwin_options.encoding > 0 ? nwin_options.encoding : 0;
	}

	if (SocketMatch) {
		/* user started us with -S option */
		sprintf(socknamebuf, "%d.%s", (int)getpid(), SocketMatch);
	} else {
		sprintf(socknamebuf, "%d.%s.%s", (int)getpid(), stripdev(attach_tty), HostName);
	}
	for (ap = socknamebuf; *ap; ap++)
		if (*ap == '/')
			*ap = '-';
	if (strlen(socknamebuf) > FILENAME_MAX) {
		socknamebuf[FILENAME_MAX] = 0;
	}
	sprintf(SocketPath + strlen(SocketPath), "/%s", socknamebuf);

	ServerSocket = MakeServerSocket();
	InitKeytab();
#ifdef ETCSCREENRC
#ifdef ALLOW_SYSSCREENRC
	if ((ap = getenv("SYSSCREENRC")))
		(void)StartRc(ap, 0);
	else
#endif
		(void)StartRc(ETCSCREENRC, 0);
#endif
	(void)StartRc(RcFileName, 0);
#ifdef ENABLE_UTMP
	InitUtmp();
#endif				/* ENABLE_UTMP */
	if (display) {
		if (InitTermcap(0, 0)) {
			fcntl(D_userfd, F_SETFL, 0);	/* Flush sets FNBLOCK */
			freetty();
			if (D_userpid)
				Kill(D_userpid, SIG_BYE);
			eexit(1);
		}
		MakeDefaultCanvas();
		InitTerm(0);
#ifdef ENABLE_UTMP
		RemoveLoginSlot();
#endif
	} else
		MakeTermcap(1);
	MakeNewEnv();
	xsignal(SIGHUP, SigHup);
	xsignal(SIGINT, FinitHandler);
	xsignal(SIGQUIT, FinitHandler);
	xsignal(SIGTERM, FinitHandler);
	xsignal(SIGTTIN, SIG_IGN);
	xsignal(SIGTTOU, SIG_IGN);

	if (display) {
		brktty(D_userfd);
		SetMode(&D_OldMode, &D_NewMode, D_flow, iflag);
		/* Note: SetMode must be called _before_ FinishRc. */
		SetTTY(D_userfd, &D_NewMode);
		if (fcntl(D_userfd, F_SETFL, FNBLOCK))
			Msg(errno, "Warning: NBLOCK fcntl failed");
	} else
		brktty(-1);	/* just try */
	xsignal(SIGCHLD, SigChld);
#ifdef ETCSCREENRC
#ifdef ALLOW_SYSSCREENRC
	if ((ap = getenv("SYSSCREENRC")))
		FinishRc(ap);
	else
#endif
		FinishRc(ETCSCREENRC);
#endif
	FinishRc(RcFileName);

	if (windows == NULL) {
		if (MakeWindow(&nwin) == -1) {
			fd_set rfd;
			FD_ZERO(&rfd);
			struct timeval tv = { MsgWait / 1000, 1000 * (MsgWait % 1000) };
			FD_SET(0, &rfd);

			Msg(0, "Sorry, could not find a PTY or TTY.");
			/* allow user to exit early by pressing any key. */
			select(1, &rfd, NULL, NULL, &tv);
			Finit(0);
			/* NOTREACHED */
		}
	} else if (argc) {	/* Screen was invoked with a command */
		MakeWindow(&nwin);
	}

	if (display && default_startup)
		display_copyright();
	xsignal(SIGINT, SigInt);
	if (rflag && (rflag & 1) == 0 && !quietflag) {
		Msg(0, "New screen...");
		rflag = 0;
	}

	serv_read.type = EV_READ;
	serv_read.fd = ServerSocket;
	serv_read.handler = serv_read_fn;
	evenq(&serv_read);

	serv_select.priority = -10;
	serv_select.type = EV_ALWAYS;
	serv_select.handler = serv_select_fn;
	evenq(&serv_select);

	logflushev.type = EV_TIMEOUT;
	logflushev.handler = logflush_fn;

	sched();
	/* NOTREACHED */
	return 0;
}

static void SigChldHandler()
{
	struct stat st;
	while (GotSigChld) {
		GotSigChld = 0;
		DoWait();
	}
	if (stat(SocketPath, &st) == -1) {
		if (!RecoverSocket()) {
			Finit(1);
		}
	}
}

static void SigChld(int sigsig)
{
	(void)sigsig; /* unused */
	GotSigChld = 1;
}

void SigHup(int sigsig)
{
	(void)sigsig; /* unused */
	/* Hangup all displays */
	while ((display = displays) != 0)
		Hangup();
}

/*
 * the backend's Interrupt handler
 * we cannot insert the intrc directly, as we never know
 * if fore is valid.
 */
static void SigInt(int sigsig)
{
	(void)sigsig; /* unused */

	xsignal(SIGINT, SigInt);

	InterruptPlease = 1;
}

static void CoreDump(int sigsig)
{
	/* if running with s-bit, we must reset the s-bit, so that we get a
	 * core file anyway.
	 */

	Display *disp;
	char buf[80];

	(void)sigsig; /* unused */

	setgid(getgid());
	setuid(getuid());
	unlink("core");

	sprintf(buf, "\r\n[screen caught a fatal signal. (core dumped)]\r\n");

	for (disp = displays; disp; disp = disp->d_next) {
		if (disp->d_nonblock < -1 || disp->d_nonblock > 1000000)
			continue;
		fcntl(disp->d_userfd, F_SETFL, 0);
		SetTTY(disp->d_userfd, &D_OldMode);
		write(disp->d_userfd, buf, strlen(buf));
		Kill(disp->d_userpid, SIG_BYE);
	}

	abort();
}

static void DoWait()
{
	int pid;
	Window *p, *next;
	int wstat;

	while ((pid = waitpid(-1, &wstat, WNOHANG | WUNTRACED)) > 0)
	{
		for (p = windows; p; p = next) {
			next = p->w_next;
			if ((p->w_pid && pid == p->w_pid) || (p->w_deadpid && pid == p->w_deadpid)) {
				/* child has ceased to exist */
				p->w_pid = 0;

				if (WIFSTOPPED(wstat)) {
#ifdef SIGTTIN
					if (WSTOPSIG(wstat) == SIGTTIN) {
						Msg(0, "Suspended (tty input)");
						continue;
					}
#endif
#ifdef SIGTTOU
					if (WSTOPSIG(wstat) == SIGTTOU) {
						Msg(0, "Suspended (tty output)");
						continue;
					}
#endif
					/* Try to restart process */
					Msg(0, "Child has been stopped, restarting.");
					if (killpg(pid, SIGCONT))
						kill(pid, SIGCONT);
				} else {
					/* Screen will detect the window has died when the window's
					 * file descriptor signals EOF (which it will do when the process in
					 * the window terminates). So do this in a timeout of 10 seconds.
					 * (not doing this at all might also work)
					 * See #27061 for more details.
					 */
					p->w_destroyev.data = (char *)p;
					p->w_exitstatus = wstat;
					SetTimeout(&p->w_destroyev, 10 * 1000);
					evenq(&p->w_destroyev);
				}
				break;
			}
			if (p->w_pwin && pid == p->w_pwin->p_pid) {
				FreePseudowin(p);
				break;
			}
		}
	}
}

static void FinitHandler(int sigsig)
{
	(void)sigsig; /* unused */

	Finit(1);
}

void Finit(int i)
{
	xsignal(SIGCHLD, SIG_DFL);
	xsignal(SIGHUP, SIG_IGN);

	while (windows) {
		Window *p = windows;
		windows = windows->w_next;
		FreeWindow(p);
	}
	if (ServerSocket != -1) {
		xseteuid(real_uid);
		xsetegid(real_gid);
		(void)unlink(SocketPath);
		xseteuid(eff_uid);
		xsetegid(eff_gid);
	}
	for (display = displays; display; display = display->d_next) {
		if (D_status)
			RemoveStatus();
		FinitTerm();
#ifdef ENABLE_UTMP
		RestoreLoginSlot();
#endif
		AddStr("[screen is terminating]\r\n");
		Flush(3);
		SetTTY(D_userfd, &D_OldMode);
		fcntl(D_userfd, F_SETFL, 0);
		freetty();
		Kill(D_userpid, SIG_BYE);
	}
	/*
	 * we _cannot_ call eexit(i) here,
	 * instead of playing with the Socket above. Sigh.
	 */
	exit(i);
}

void eexit(int e)
{
	if (ServerSocket != -1) {
		setgid(real_gid);
		setuid(real_uid);
		(void)unlink(SocketPath);
	}
	exit(e);
}

void Hangup()
{
	if (display == 0)
		return;
	if (D_userfd >= 0) {
		close(D_userfd);
		D_userfd = -1;
	}
	if (auto_detach || displays->d_next)
		Detach(D_HANGUP);
	else
		Finit(0);
}

/*
 * Detach now has the following modes:
 *D_DETACH	 SIG_BYE	detach backend and exit attacher
 *D_HANGUP	 SIG_BYE	detach backend and exit attacher
 *D_STOP	 SIG_STOP	stop attacher (and detach backend)
 *D_REMOTE	 SIG_BYE	remote detach -- reattach to new attacher
 *D_POWER 	 SIG_POWER_BYE 	power detach -- attacher kills his parent
 *D_REMOTE_POWER SIG_POWER_BYE	remote power detach -- both
 *D_LOCK	 SIG_LOCK	lock the attacher
 * (jw)
 * we always remove our utmp slots. (even when "lock" or "stop")
 * Note: Take extra care here, we may be called by interrupt!
 */
void Detach(int mode)
{
	int sign = 0, pid;
	Canvas *cv;
	Window *p;

	if (display == 0)
		return;

#define AddStrSocket(msg) do { \
    if (SocketName) \
      { \
	AddStr("[" msg " from "); \
	AddStr(SocketName); \
	AddStr("]\r\n"); \
      } \
    else \
      AddStr("[" msg "]\r\n"); \
  } while (0)

	xsignal(SIGHUP, SIG_IGN);
	if (D_status)
		RemoveStatus();
	FinitTerm();
	if (!display)
		return;
	switch (mode) {
	case D_HANGUP:
		sign = SIG_BYE;
		break;
	case D_DETACH:
		AddStrSocket("detached");
		sign = SIG_BYE;
		break;
	case D_STOP:
		sign = SIG_STOP;
		break;
	case D_REMOTE:
		AddStrSocket("remote detached");
		sign = SIG_BYE;
		break;
	case D_POWER:
		AddStrSocket("power detached");
		if (PowDetachString) {
			AddStr(PowDetachString);
			AddStr("\r\n");
		}
		sign = SIG_POWER_BYE;
		break;
	case D_REMOTE_POWER:
		AddStrSocket("remote power detached");
		if (PowDetachString) {
			AddStr(PowDetachString);
			AddStr("\r\n");
		}
		sign = SIG_POWER_BYE;
		break;
	case D_LOCK:
		ClearAll();
		ClearScrollbackBuffer();
		sign = SIG_LOCK;
		/* tell attacher to lock terminal with a lockprg. */
		break;
	}
#ifdef ENABLE_UTMP
	if (displays->d_next == 0) {
		for (p = windows; p; p = p->w_next) {
			if (p->w_slot != (slot_t) - 1 && !(p->w_lflag & 2)) {
				RemoveUtmp(p);
				/*
				 * Set the slot to 0 to get the window
				 * logged in again.
				 */
				p->w_slot = (slot_t) 0;
			}
		}
	}
	if (mode != D_HANGUP)
		RestoreLoginSlot();
#endif
	if (displays->d_next == 0 && console_window) {
		if (TtyGrabConsole(console_window->w_ptyfd, false, "detach")) {
			KillWindow(console_window);
			display = displays;	/* restore display */
		}
	}
	if (D_fore) {
		ReleaseAutoWritelock(display, D_fore);
		D_user->u_detachwin = D_fore->w_number;
		D_user->u_detachotherwin = D_other ? D_other->w_number : D_fore->w_number;
	}
	AutosaveLayout(D_layout);
	layout_last = D_layout;
	for (cv = D_cvlist; cv; cv = cv->c_next) {
		p = Layer2Window(cv->c_layer);
		SetCanvasWindow(cv, 0);
		if (p)
			WindowChanged(p, 'u');
	}

	pid = D_userpid;
	FreeDisplay();
	if (displays == 0)
		/* Flag detached-ness */
		(void)chsock();
	/*
	 * tell father what to do. We do that after we
	 * freed the tty, thus getty feels more comfortable on hpux
	 * if it was a power detach.
	 */
	Kill(pid, sign);
	xsignal(SIGHUP, SigHup);
#undef AddStrSocket
}

static int IsSymbol(char *e, char *s)
{
	int l;

	l = strlen(s);
	return strncmp(e, s, l) == 0 && e[l] == '=';
}

void MakeNewEnv()
{
	char **op, **np;
	static char stybuf[MAXSTR];

	for (op = environ; *op; ++op) ;
	if (NewEnv)
		free((char *)NewEnv);
	NewEnv = np = malloc((unsigned)(op - environ + 7 + 1) * sizeof(char *));
	if (!NewEnv)
		Panic(0, "%s", strnomem);
	sprintf(stybuf, "STY=%s", strlen(SocketName) <= MAXSTR - 5 ? SocketName : "?");
	*np++ = stybuf;		/* NewEnv[0] */
	*np++ = Term;		/* NewEnv[1] */
	np++;			/* room for SHELL */
	np += 2;		/* room for TERMCAP and WINDOW */

	for (op = environ; *op; ++op) {
		if (!IsSymbol(*op, "TERM") && !IsSymbol(*op, "TERMCAP")
		    && !IsSymbol(*op, "STY") && !IsSymbol(*op, "WINDOW")
		    && !IsSymbol(*op, "SCREENCAP") && !IsSymbol(*op, "SHELL")
		    && !IsSymbol(*op, "LINES") && !IsSymbol(*op, "COLUMNS")
		    )
			*np++ = *op;
	}
	*np = 0;
}

#define	PROCESS_MESSAGE(B) do { \
    char *p = B;	\
    va_list ap;	\
    va_start(ap, fmt);	\
    (void)vsnprintf(p, sizeof(B) - 100, fmt, ap);	\
    va_end(ap);	\
    if (err)	\
      {	\
	p += strlen(p);	\
	*p++ = ':';	\
	*p++ = ' ';	\
	strncpy(p, strerror(err), B + sizeof(B) - p - 1);	\
	B[sizeof(B) - 1] = 0;	\
      }	\
  } while (0)

void Msg(int err, const char *fmt, ...)
{
	char buf[MAXPATHLEN * 2];
	PROCESS_MESSAGE(buf);

	if (display && displays)
		MakeStatus(buf);
	else if (displays) {
		for (display = displays; display; display = display->d_next)
			MakeStatus(buf);
	} else if (display) {
		/* no displays but a display - must have forked.
		 * send message to backend!
		 */
		char *tty = D_usertty;
		Display *olddisplay = display;
		display = 0;	/* only send once */
		SendErrorMsg(tty, buf);
		display = olddisplay;
	} else
		printf("%s\r\n", buf);

	if (queryflag >= 0)
		write(queryflag, buf, strlen(buf));
}

/*
 * Call FinitTerm for all displays, write a message to each and call eexit();
 */
void Panic(int err, const char *fmt, ...)
{
	char buf[MAXPATHLEN * 2];
	PROCESS_MESSAGE(buf);

	if (displays == 0 && display == 0) {
		printf("%s\r\n", buf);
		if (PanicPid)
			Kill(PanicPid, SIG_BYE);
	} else if (displays == 0) {
		/* no displays but a display - must have forked.
		 * send message to backend!
		 */
		char *tty = D_usertty;
		display = 0;
		SendErrorMsg(tty, buf);
		sleep(2);
		_exit(1);
	} else
		for (display = displays; display; display = display->d_next) {
			if (D_status)
				RemoveStatus();
			FinitTerm();
			Flush(3);
#ifdef ENABLE_UTMP
			RestoreLoginSlot();
#endif
			SetTTY(D_userfd, &D_OldMode);
			fcntl(D_userfd, F_SETFL, 0);
			write(D_userfd, buf, strlen(buf));
			write(D_userfd, "\n", 1);
			freetty();
			if (D_userpid)
				Kill(D_userpid, SIG_BYE);
		}
	if (tty_oldmode >= 0) {
#ifdef USE_SETEUID
		if (setuid(own_uid))
			xseteuid(own_uid);	/* may be a loop. sigh. */
#else
		setuid(own_uid);
#endif
		chmod(attach_tty, tty_oldmode);
	}
	eexit(1);
}

void QueryMsg(int err, const char *fmt, ...)
{
	char buf[MAXPATHLEN * 2];

	if (queryflag < 0)
		return;

	PROCESS_MESSAGE(buf);
	write(queryflag, buf, strlen(buf));
}

void Dummy(int err, const char *fmt, ...)
{
	(void)err; /* unused */
	(void)fmt; /* unused */
}

#undef PROCESS_MESSAGE

/*
 * '^' is allowed as an escape mechanism for control characters. jw.
 *
 * Added time insertion using ideas/code from /\ndy Jones
 *   (andy@lingua.cltr.uq.OZ.AU) - thanks a lot!
 *
 */

void PutWinMsg(char *s, int start, int max)
{
	int i, p, l, n;
	uint64_t r;
	struct mchar rend;
	struct mchar rendstack[MAX_WINMSG_REND];
	int rendstackn = 0;

	if (s != g_winmsg->buf) {
		/* sorry, no fancy coloring available */
		l = strlen(s);
		if (l > max)
			l = max;
		l -= start;
		s += start;
		while (l-- > 0)
			PUTCHARLP(*s++);
		return;
	}
	rend = D_rend;
	p = 0;
	l = strlen(s);
	for (i = 0; i < g_winmsg->numrend && max > 0; i++) {
		if (p > g_winmsg->rendpos[i] || g_winmsg->rendpos[i] > l)
			break;
		if (p < g_winmsg->rendpos[i]) {
			n = g_winmsg->rendpos[i] - p;
			if (n > max)
				n = max;
			max -= n;
			p += n;
			while (n-- > 0) {
				if (start-- > 0)
					s++;
				else
					PUTCHARLP(*s++);
			}
		}
		r = g_winmsg->rend[i];
		if (r == 0) {
			if (rendstackn > 0)
				rend = rendstack[--rendstackn];
		} else {
			rendstack[rendstackn++] = rend;
			ApplyAttrColor(r, &rend);
		}
		SetRendition(&rend);
	}
	if (p < l) {
		n = l - p;
		if (n > max)
			n = max;
		while (n-- > 0) {
			if (start-- > 0)
				s++;
			else
				PUTCHARLP(*s++);
		}
	}
}

static void serv_read_fn(Event *event, void *data)
{
	(void)event; /* unused */
	(void)data; /* unused */

	ReceiveMsg();
}

static void serv_select_fn(Event *event, void *data)
{
	Window *p;

	(void)event; /* unused */
	(void)data; /* unused */

	/* XXX: messages?? */
	if (GotSigChld) {
		SigChldHandler();
	}
	if (InterruptPlease) {
		/* This approach is rather questionable in a multi-display
		 * environment */
		if (fore && displays) {
			char ibuf = displays->d_OldMode.tio.c_cc[VINTR];
			write(W_UWP(fore) ? fore->w_pwin->p_ptyfd : fore->w_ptyfd, &ibuf, 1);
		}
		InterruptPlease = 0;
	}

	for (p = windows; p; p = p->w_next) {
		if (p->w_bell == BELL_FOUND || p->w_bell == BELL_VISUAL) {
			Canvas *cv;
			int visual = p->w_bell == BELL_VISUAL || visual_bell;
			p->w_bell = BELL_ON;
			for (display = displays; display; display = display->d_next) {
				for (cv = D_cvlist; cv; cv = cv->c_next)
					if (cv->c_layer->l_bottom == &p->w_layer)
						break;
				if (cv == 0) {
					p->w_bell = BELL_DONE;
					Msg(0, "%s", MakeWinMsg(BellString, p, '%'));
				} else if (visual && !D_VB && (!D_status || !D_status_bell)) {
					Msg(0, "%s", VisualBellString);
					if (D_status) {
						D_status_bell = 1;
						SetTimeout(&D_statusev, VBellWait);
					}
				}
			}
			/* don't annoy the user with two messages */
			if (p->w_monitor == MON_FOUND)
				p->w_monitor = MON_DONE;
			WindowChanged(p, 'f');
		}
		if (p->w_monitor == MON_FOUND) {
			Canvas *cv;
			p->w_monitor = MON_ON;
			for (display = displays; display; display = display->d_next) {
				for (cv = D_cvlist; cv; cv = cv->c_next)
					if (cv->c_layer->l_bottom == &p->w_layer)
						break;
				if (cv)
					continue;	/* user already sees window */
				if (!(ACLBYTE(p->w_mon_notify, D_user->u_id) & ACLBIT(D_user->u_id)))
					continue;	/* user doesn't care */
				Msg(0, "%s", MakeWinMsg(ActivityString, p, '%'));
				p->w_monitor = MON_DONE;
			}
			WindowChanged(p, 'f');
		}
		if (p->w_silence == SILENCE_FOUND) {
			/* Unset the flag if the user switched to this window. */
			if (p->w_layer.l_cvlist) {
				p->w_silence = SILENCE_ON;
				WindowChanged(p, 'f');
			}
		}
	}

	for (display = displays; display; display = display->d_next) {
		Canvas *cv;
		if (D_status == STATUS_ON_WIN)
			continue;
		/* XXX: should use display functions! */
		for (cv = D_cvlist; cv; cv = cv->c_next) {
			int lx, ly;

			/* normalize window, see resize.c */
			lx = cv->c_layer->l_x;
			ly = cv->c_layer->l_y;
			if (lx == cv->c_layer->l_width)
				lx--;
			if (ly + cv->c_yoff < cv->c_ys) {
				int i, n = cv->c_ys - (ly + cv->c_yoff);
				cv->c_yoff = cv->c_ys - ly;
				RethinkViewportOffsets(cv);
				if (n > cv->c_layer->l_height)
					n = cv->c_layer->l_height;
				CV_CALL(cv,
					LScrollV(flayer, -n, 0, flayer->l_height - 1, 0);
					LayRedisplayLine(-1, -1, -1, 1); for (i = 0; i < n; i++)
					LayRedisplayLine(i, 0, flayer->l_width - 1, 1);
					if (cv == cv->c_display->d_forecv)
					LaySetCursor();) ;
			} else if (ly + cv->c_yoff > cv->c_ye) {
				int i, n = ly + cv->c_yoff - cv->c_ye;
				cv->c_yoff = cv->c_ye - ly;
				RethinkViewportOffsets(cv);
				if (n > cv->c_layer->l_height)
					n = cv->c_layer->l_height;
				CV_CALL(cv,
					LScrollV(flayer, n, 0, cv->c_layer->l_height - 1, 0);
					LayRedisplayLine(-1, -1, -1, 1); for (i = 0; i < n; i++)
					LayRedisplayLine(i + flayer->l_height - n, 0, flayer->l_width - 1, 1);
					if (cv == cv->c_display->d_forecv)
					LaySetCursor();) ;
			}
			if (lx + cv->c_xoff < cv->c_xs) {
				int i, n = cv->c_xs - (lx + cv->c_xoff);
				if (n < (cv->c_xe - cv->c_xs + 1) / 2)
					n = (cv->c_xe - cv->c_xs + 1) / 2;
				if (cv->c_xoff + n > cv->c_xs)
					n = cv->c_xs - cv->c_xoff;
				cv->c_xoff += n;
				RethinkViewportOffsets(cv);
				if (n > cv->c_layer->l_width)
					n = cv->c_layer->l_width;
				CV_CALL(cv, LayRedisplayLine(-1, -1, -1, 1); for (i = 0; i < flayer->l_height; i++) {
					LScrollH(flayer, -n, i, 0, flayer->l_width - 1, 0, 0);
					LayRedisplayLine(i, 0, n - 1, 1);}
					if (cv == cv->c_display->d_forecv)
					LaySetCursor();) ;
			} else if (lx + cv->c_xoff > cv->c_xe) {
				int i, n = lx + cv->c_xoff - cv->c_xe;
				if (n < (cv->c_xe - cv->c_xs + 1) / 2)
					n = (cv->c_xe - cv->c_xs + 1) / 2;
				if (cv->c_xoff - n + cv->c_layer->l_width - 1 < cv->c_xe)
					n = cv->c_xoff + cv->c_layer->l_width - 1 - cv->c_xe;
				cv->c_xoff -= n;
				RethinkViewportOffsets(cv);
				if (n > cv->c_layer->l_width)
					n = cv->c_layer->l_width;
				CV_CALL(cv, LayRedisplayLine(-1, -1, -1, 1); for (i = 0; i < flayer->l_height; i++) {
					LScrollH(flayer, n, i, 0, flayer->l_width - 1, 0, 0);
					LayRedisplayLine(i, flayer->l_width - n, flayer->l_width - 1, 1);}
					if (cv == cv->c_display->d_forecv)
					LaySetCursor();) ;
			}
		}
	}

	for (display = displays; display; display = display->d_next) {
		if (D_status == STATUS_ON_WIN || D_cvlist == 0 || D_cvlist->c_next == 0)
			continue;
		CV_CALL(D_forecv, LayRestore();
			LaySetCursor());
	}
}

static void logflush_fn(Event *event, void *data)
{
	Window *p;
	char *buf;
	int n;

	(void)data; /* unused */

	if (!islogfile(NULL))
		return;		/* no more logfiles */
	logfflush(NULL);
	n = log_flush ? log_flush : (logtstamp_after + 4) / 5;
	if (n) {
		SetTimeout(event, n * 1000);
		evenq(event);	/* re-enqueue ourself */
	}
	if (!logtstamp_on)
		return;
	/* write fancy time-stamp */
	for (p = windows; p; p = p->w_next) {
		if (!p->w_log)
			continue;
		p->w_logsilence += n;
		if (p->w_logsilence < logtstamp_after)
			continue;
		if (p->w_logsilence - n >= logtstamp_after)
			continue;
		buf = MakeWinMsg(logtstamp_string, p, '%');
		logfwrite(p->w_log, buf, strlen(buf));
	}
}

/*
 * Interprets ^?, ^@ and other ^-control-char notation.
 * Interprets \ddd octal notation
 *
 * The result is placed in *cp, p is advanced behind the parsed expression and
 * returned.
 */
static char *ParseChar(char *p, char *cp)
{
	if (*p == 0)
		return 0;
	if (*p == '^' && p[1]) {
		if (*++p == '?')
			*cp = '\177';
		else if (*p >= '@')
			*cp = Ctrl(*p);
		else
			return 0;
		++p;
	} else if (*p == '\\' && *++p <= '7' && *p >= '0') {
		*cp = 0;
		do
			*cp = *cp * 8 + *p - '0';
		while (*++p <= '7' && *p >= '0');
	} else
		*cp = *p++;
	return p;
}

static int ParseEscape(char *p)
{
	unsigned char buf[2];

	if (*p == 0)
		SetEscape((struct acluser *)0, -1, -1);
	else {
		if ((p = ParseChar(p, (char *)buf)) == NULL || (p = ParseChar(p, (char *)buf + 1)) == NULL || *p)
			return -1;
		SetEscape((struct acluser *)0, buf[0], buf[1]);
	}
	return 0;
}

