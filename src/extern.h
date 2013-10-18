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

#include <stdbool.h>

/*
 * GLOBAL VARIABLES
 */

extern char screenterm[];
extern char version[];
extern char DefaultShell[];
extern char HostName[];
extern char NullStr[];
extern char SocketPath[];
extern char Term[];
extern char Termcap[];
extern char *attach_tty;
extern char *attach_term;
extern char *captionstring;
extern char *extra_incap;
extern char *extra_outcap;
extern char *hardcopydir;
extern char *home;
extern char *hstatusstring;
extern char *kmapadef[];
extern char *kmapdef[];
extern char *kmapmdef[];
extern char *logtstamp_string;
extern char *multi;
extern char *noargs[];
extern char *preselect;
extern char *printcmd;
extern char *rc_name;
extern char *screenencodings;
extern char *screenlogfile;
extern char *timestring;
extern char *wliststr;
extern char *wlisttit;
extern char *ActivityString;
extern char *BellString;
extern char *BufferFile;
extern char *LoginName;
extern char *PowDetachString;
extern char *RcFileName;
extern char *ShellArgs[];
extern char *ShellProg;
extern char *SocketMatch;
extern char *SocketName;
extern char *VisualBellString;
extern char **environ;
extern char **NewEnv;

extern unsigned char mark_key_tab[];
extern uint32_t *blank;
extern uint32_t *null;

extern bool adaptflag;
extern int attach_fd;
extern int auto_detach;
extern int captionalways;
extern int breaktype;
extern int cjkwidth;
extern int compacthist;
extern int default_startup;
extern int defautonuke;
extern int defmousetrack;
extern int defnonblock;
extern int defobuflimit;
extern int dflag;
extern int focusminheight;
extern int focusminwidth;
extern int force_vt;
extern int hardcopy_append;
extern int hardstatusemu;
extern int idletimo;
extern bool iflag;
extern int join_with_cr;
extern int kmap_extn;
extern bool lsflag;
extern int log_flush;
extern int logtstamp_on;
extern int logtstamp_after;
extern int maxusercount;
extern int maxwin;
extern int multi_uid;
extern int multiattach;
extern int nversion;
extern int own_uid;
extern int pastefont;
extern int pty_preopen;
extern int search_ic;
extern int separate_sids;
extern int queryflag;
extern bool quietflag;
extern uint64_t renditions[];
extern int rflag;
extern int tty_mode;
extern int tty_oldmode;
extern int use_altscreen;
extern int use_hardstatus;
extern bool wipeflag;
extern bool xflag;
extern int visual_bell;
extern int DefaultEsc;
extern int MasterPid;
extern int MsgMinWait;
extern int MsgWait;
extern int ServerSocket;
extern int SilenceWait;
extern int TtyMode;
extern int VerboseCreate;
extern int VBellWait;
extern int Z0width;
extern int Z1width;
extern int ZombieKey_destroy;
extern int ZombieKey_onerror;
extern int ZombieKey_resurrect;

extern struct action idleaction;
extern struct action dmtab[];
extern struct action ktab[];
extern struct action mmtab[];
extern struct action umtab[];
extern struct term term[];
extern struct acluser *users, *EffectiveAclUser;
extern Display *display, *displays;
extern struct LayFuncs ListLf;
extern struct LayFuncs MarkLf;
extern struct LayFuncs WinLf;
extern struct LayFuncs BlankLf;
extern struct layout *layout_attach, *layout_last, layout_last_marker;
extern struct layout *layouts;
extern struct layout *laytab[];
extern struct NewWindow nwin_undef, nwin_default, nwin_options;
extern Window *fore, **wtab, *console_window, *windows;
extern struct kmap_ext *kmap_exts;
extern int kmap_extn;
extern Layer *flayer;
extern struct mline mline_blank;
extern struct mline mline_null;
extern struct mline mline_old;
extern struct mchar mchar_so;
extern struct mchar mchar_blank;
extern struct mchar mchar_null;
extern struct comm comms[];
extern Event logflushev;
extern Event serv_read;
extern struct mode attach_Mode;
extern struct passwd *ppp;

extern gid_t eff_gid;
extern gid_t real_gid;
extern uid_t eff_uid;
extern uid_t real_uid;

#if defined(TIOCSWINSZ) || defined(TIOCGWINSZ)
extern struct winsize glwz;
#endif

#ifdef O_NOCTTY
extern int separate_sids;
#endif

extern struct utmp *getutline(), *pututline();

