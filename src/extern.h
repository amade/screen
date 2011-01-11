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

#if !defined(__GNUC__) || __GNUC__ < 2
#undef __attribute__
#define __attribute__(x)
#endif

/* screen.c */
extern int   main (int, char **);
extern sigret_t SigHup SIGPROTOARG;
extern void  eexit (int) __attribute__((__noreturn__));
extern void  Detach (int);
extern void  Hangup (void);
extern void  Kill (int, int);
extern void  Msg (int, const char *, ...) __attribute__((format(printf, 2, 3)));
extern void  Panic (int, const char *, ...) __attribute__((format(printf, 2, 3))) __attribute__((__noreturn__));
extern void  QueryMsg (int, const char *, ...) __attribute__((format(printf, 2, 3)));
extern void  Dummy (int, const char *, ...) __attribute__((format(printf, 2, 3)));
extern void  Finit (int);
extern void  MakeNewEnv (void);
extern char *MakeWinMsg (char *, struct win *, int);
extern char *MakeWinMsgEv (char *, struct win *, int, int, struct event *, int);
extern int   AddWinMsgRend (const char *, int);
extern void  PutWinMsg (char *, int, int);
#ifdef BSDWAIT
extern void  WindowDied (struct win *, union wait, int);
#else
extern void  WindowDied (struct win *, int, int);
#endif
extern void  setbacktick (int, int, int, char **);

/* ansi.c */
extern void  ResetAnsiState (struct win *);
extern void  ResetWindow (struct win *);
extern void  ResetCharsets (struct win *);
extern void  WriteString (struct win *, char *, int);
extern void  ChangeAKA (struct win *, char *, int);
extern void  SetCharsets (struct win *, char *);
extern int   GetAnsiStatus (struct win *, char *);
extern void  WNewAutoFlow (struct win *, int);
extern void  WBell (struct win *, int);
extern void  WMsg (struct win *, int, char *);
extern void  WChangeSize (struct win *, int, int);
extern void  WindowChanged (struct win *, int);
extern int   MFindUsedLine (struct win *, int, int);

/* fileio.c */
extern int   StartRc (char *, int);
extern void  FinishRc (char *);
extern void  RcLine (char *, int);
extern FILE *secfopen (char *, char *);
extern int   secopen (char *, int, int);
extern void  WriteFile (struct acluser *, char *, int);
extern char *ReadFile (char *, int *);
extern void  KillBuffers (void);
extern int   printpipe (struct win *, char *);
extern int   readpipe (char **);
extern void  RunBlanker (char **);
extern void  do_source (char *);

/* tty.c */
extern int   OpenTTY (char *, char *);
extern void  InitTTY (struct mode *, int);
extern void  GetTTY (int, struct mode *);
extern void  SetTTY (int, struct mode *);
extern void  SetMode (struct mode *, struct mode *, int, int);
extern void  SetFlow (int);
extern void  SendBreak (struct win *, int, int);
extern int   TtyGrabConsole (int, int, char *);
extern char *TtyGetModemStatus (int, char *);
#ifdef DEBUG
extern void  DebugTTY (struct mode *);
#endif /* DEBUG */
extern int   fgtty (int);
extern void  brktty (int);
extern struct baud_values *lookup_baud (int bps);
extern int   SetBaud (struct mode *, int, int);
extern int   SttyMode (struct mode *, char *);
extern int   CheckTtyname (char *);

/* mark.c */
extern int   GetHistory (void);
extern void  MarkRoutine (void);
extern void  revto_line (int, int, int);
extern void  revto (int, int);
extern int   InMark (void);
extern void  MakePaster (struct paster *, char *, int, int);
extern void  FreePaster (struct paster *);

/* search.c */
extern void  Search (int);
extern void  ISearch (int);

/* input.c */
extern void  inp_setprompt (char *, char *);
extern void  Input (char *, int, int, void (*)(char *, int, char *), char *, int);
extern int   InInput (void);

/* help.c */
extern void  exit_with_usage (char *, char *, char *);
extern void  display_help (char *, struct action *);
extern void  display_copyright (void);
extern void  display_displays (void);
extern void  display_bindkey (char *, struct action *);
extern int   InWList (void);
extern void  WListUpdatecv (struct canvas *, struct win *);
extern void  WListLinkChanged (void);
extern void  ZmodemPage (void);

/* window.c */
extern int   MakeWindow (struct NewWindow *);
extern int   RemakeWindow (struct win *);
extern void  FreeWindow (struct win *);
extern int   winexec (char **);
extern void  FreePseudowin (struct win *);
extern void  nwin_compose (struct NewWindow *, struct NewWindow *, struct NewWindow *);
extern int   DoStartLog (struct win *, char *, int);
extern int   ReleaseAutoWritelock (struct display *, struct win *);
extern int   ObtainAutoWritelock (struct display *, struct win *);
extern void  CloseDevice (struct win *);
extern void  zmodem_abort (struct win *, struct display *);
#ifndef HAVE_EXECVPE
extern void  execvpe (char *, char **, char **);
#endif

/* utmp.c */
#ifdef UTMPOK
extern void  InitUtmp (void);
extern void  RemoveLoginSlot (void);
extern void  RestoreLoginSlot (void);
extern int   SetUtmp (struct win *);
extern int   RemoveUtmp (struct win *);
#endif /* UTMPOK */
extern void  SlotToggle (int);
#ifdef USRLIMIT
extern int   CountUsers (void);
#endif
#ifdef CAREFULUTMP
extern void   CarefulUtmp (void);
#else
# define CarefulUtmp()  /* nothing */
#endif /* CAREFULUTMP */


/* loadav.c */
#ifdef LOADAV
extern void  InitLoadav (void);
extern void  AddLoadav (char *);
#endif

/* pty.c */
extern int   OpenPTY (char **);
extern void  InitPTY (int);

/* process.c */
extern void  InitKeytab (void);
extern void  ProcessInput (char *, int);
extern void  ProcessInput2 (char *, int);
extern void  DoProcess (struct win *, char **, int *, struct paster *);
extern void  DoAction  (struct action *, int);
extern int   FindCommnr (const char *);
extern void  DoCommand (char **, int *);
extern void  Activate (int);
extern void  KillWindow (struct win *);
extern void  SetForeWindow (struct win *);
extern int   Parse (char *, int, char **, int *);
extern void  SetEscape (struct acluser *, int, int);
extern void  DoScreen (char *, char **);
extern int   IsNumColon (char *, int, char *, int);
extern void  ShowWindows (int);
extern char *AddWindows (char *, int, int, int);
extern char *AddWindowFlags (char *, int, struct win *);
extern char *AddOtherUsers (char *, int, struct win *);
extern int   WindowByNoN (char *);
extern struct win *FindNiceWindow (struct win *, char *);
extern int   CompileKeys (char *, int, unsigned char *);
extern void  RefreshXtermOSC (void);
extern int   ParseSaveStr (struct action *act, char **);
extern int   ParseNum (struct action *act, int *);
extern int   ParseSwitch (struct action *, int *);
extern int   ParseAttrColor (char *, char *, int);
extern void  ApplyAttrColor (int, struct mchar *);
extern void  SwitchWindow (int);
extern int   StuffKey (int);

/* termcap.c */
extern int   InitTermcap (int, int);
extern char *MakeTermcap (int);
extern char *gettermcapstring (char *);
extern int   remap (int, int);
extern void  CheckEscape (void);
extern int   CreateTransTable (char *);
extern void  FreeTransTable (void);

/* attacher.c */
extern int   Attach (int);
extern void  Attacher (void);
extern sigret_t AttacherFinit SIGPROTOARG;
extern void  SendCmdMessage (char *, char *, char **, int);

/* display.c */
extern struct display *MakeDisplay (char *, char *, char *, int, int, struct mode *);
extern void  FreeDisplay (void);
extern void  DefProcess (char **, int *);
extern void  DefRedisplayLine (int, int, int, int);
extern void  DefClearLine (int, int, int, int);
extern int   DefRewrite (int, int, int, struct mchar *, int);
extern int   DefResize (int, int);
extern void  DefRestore (void);
extern void  AddCStr (char *);
extern void  AddCStr2 (char *, int);
extern void  InitTerm (int);
extern void  FinitTerm (void);
extern void  PUTCHAR (int);
extern void  PUTCHARLP (int);
extern void  ClearAll (void);
extern void  ClearArea (int, int, int, int, int, int, int, int);
extern void  ClearLine (struct mline *, int, int, int, int);
extern void  RefreshAll (int);
extern void  RefreshArea (int, int, int, int, int);
extern void  RefreshLine (int, int, int, int);
extern void  Redisplay (int);
extern void  RedisplayDisplays (int);
extern void  ShowHStatus (char *);
extern void  RefreshHStatus (void);
extern void  DisplayLine (struct mline *, struct mline *, int, int, int);
extern void  GotoPos (int, int);
extern int   CalcCost (char *);
extern void  ScrollH (int, int, int, int, int, struct mline *);
extern void  ScrollV (int, int, int, int, int, int);
extern void  PutChar (struct mchar *, int, int);
extern void  InsChar (struct mchar *, int, int, int, struct mline *);
extern void  WrapChar (struct mchar *, int, int, int, int, int, int, int);
extern void  ChangeScrollRegion (int, int);
extern void  InsertMode (int);
extern void  KeypadMode (int);
extern void  CursorkeysMode (int);
extern void  ReverseVideo (int);
extern void  CursorVisibility (int);
extern void  MouseMode (int);
extern void  SetFont (int);
extern void  SetAttr (int);
extern void  SetColor (int, int);
extern void  SetRendition (struct mchar *);
extern void  SetRenditionMline (struct mline *, int);
extern void  MakeStatus (char *);
extern void  RemoveStatus (void);
extern int   ResizeDisplay (int, int);
extern void  AddStr (char *);
extern void  AddStrn (char *, int);
extern void  Flush (int);
extern void  freetty (void);
extern void  Resize_obuf (void);
extern void  NukePending (void);
extern void  ClearAllXtermOSC (void);
extern void  SetXtermOSC (int, char *);
extern int   color256to16 (int);
extern int   color256to88 (int);
extern void  ResetIdle (void);
extern void  KillBlanker (void);
extern void  DisplaySleep1000 (int, int);

/* resize.c */
extern int   ChangeWindowSize (struct win *, int, int, int);
extern void  ChangeScreenSize (int, int, int);
extern void  CheckScreenSize (int);
extern char *xrealloc (char *, int);
extern void  ResizeLayersToCanvases (void);
extern void  ResizeLayer (struct layer *, int, int, struct display *);
extern int   MayResizeLayer (struct layer *);
extern void  FreeAltScreen (struct win *);
extern void  EnterAltScreen (struct win *);
extern void  LeaveAltScreen (struct win *);

/* sched.c */
extern void  evenq (struct event *);
extern void  evdeq (struct event *);
extern void  SetTimeout (struct event *, int);
extern void  sched (void);

/* socket.c */
extern int   FindSocket (int *, int *, int *, char *);
extern int   MakeClientSocket (int);
extern int   MakeServerSocket (void);
extern int   RecoverSocket (void);
extern int   chsock (void);
extern void  ReceiveMsg (void);
extern void  SendCreateMsg (char *, struct NewWindow *);
extern int   SendErrorMsg (char *, char *);
extern int   SendAttachMsg (int, struct msg *, int);
extern void  ReceiveRaw (int);

/* misc.c */
extern char *SaveStr (const char *);
extern char *SaveStrn (const char *, int);
extern char *InStr (char *, const char *);
#ifndef HAVE_STRERROR
extern char *strerror (int);
#endif
extern void  centerline (char *, int);
extern void  leftline (char *, int, struct mchar *);
extern char *Filename (char *);
extern char *stripdev (char *);
extern void  bclear (char *, int);
extern void  closeallfiles (int);
extern int   UserContext (void);
extern void  UserReturn (int);
extern int   UserStatus (void);
#if defined(POSIX) || defined(hpux)
extern void (*xsignal (int, void (*)SIGPROTOARG)) SIGPROTOARG;
#endif
#ifndef HAVE_RENAME
extern int   rename (char *, char *);
#endif
#if defined(HAVE_SETEUID) || defined(HAVE_SETREUID)
extern void  xseteuid  (int);
extern void  xsetegid  (int);
#endif
extern int   AddXChar (char *, int);
extern int   AddXChars (char *, int, char *);
extern void  xsetenv  (char *, char *);
extern void  sleep1000 (int);
#ifdef DEBUG
extern void  opendebug (int, int);
#endif


/* acl.c */
extern int   AclCheckPermWin (struct acluser *, int, struct win *);
extern int   AclCheckPermCmd (struct acluser *, int, struct comm *);
extern int   AclSetPerm (struct acluser *, struct acluser *, char *, char *);
extern int   AclUmask (struct acluser *, char *, char **);
extern int   UsersAcl (struct acluser *, int, char **);
extern void  AclWinSwap (int, int);
extern int   NewWindowAcl (struct win *, struct acluser *);
extern void  FreeWindowAcl (struct win *);
extern char *DoSu (struct acluser **, char *, char *, char *);
extern int   AclLinkUser (char *, char *);
extern int   UserFreeCopyBuffer (struct acluser *);
extern struct acluser **FindUserPtr (char *);
extern int   UserAdd (char *, char *, struct acluser **);
extern int   UserDel (char *, struct acluser **);


/* layer.c */
extern void  LGotoPos (struct layer *, int, int);
extern void  LPutChar (struct layer *, struct mchar *, int, int);
extern void  LInsChar (struct layer *, struct mchar *, int, int, struct mline *);
extern void  LPutStr (struct layer *, char *, int, struct mchar *, int, int);
extern void  LPutWinMsg (struct layer *, char *, int, struct mchar *, int, int);
extern void  LScrollH (struct layer *, int, int, int, int, int, struct mline *);
extern void  LScrollV (struct layer *, int, int, int, int);
extern void  LClearAll (struct layer *, int);
extern void  LClearArea (struct layer *, int, int, int, int, int, int);
extern void  LClearLine (struct layer *, int, int, int, int, struct mline *);
extern void  LRefreshAll (struct layer *, int);
extern void  LCDisplayLine (struct layer *, struct mline *, int, int, int, int);
extern void  LCDisplayLineWrap (struct layer *, struct mline *, int, int, int, int);
extern void  LSetRendition (struct layer *, struct mchar *);
extern void  LWrapChar  (struct layer *, struct mchar *, int, int, int, int);
extern void  LCursorVisibility (struct layer *, int);
extern void  LSetFlow (struct layer *, int);
extern void  LKeypadMode (struct layer *, int);
extern void  LCursorkeysMode (struct layer *, int);
extern void  LMouseMode (struct layer *, int);
extern void  LMsg (int, const char *, ...) __attribute__((format(printf, 2, 3)));
extern void  KillLayerChain (struct layer *);
extern int   InitOverlayPage (int, struct LayFuncs *, int);
extern void  ExitOverlayPage (void);
extern int   LayProcessMouse (struct layer *, unsigned char);
extern void  LayProcessMouseSwitch (struct layer *, int);

/* teln.c */
#ifdef BUILTIN_TELNET
extern int   TelOpen (char **);
extern int   TelConnect (struct win *);
extern int   TelIsline (struct win *p);
extern void  TelProcessLine (char **, int *);
extern int   DoTelnet (char *, int *, int);
extern int   TelIn (struct win *, char *, int, int);
extern void  TelBreak (struct win *);
extern void  TelWindowSize (struct win *);
extern void  TelStatus (struct win *, char *, int);
#endif

/* nethack.c */
extern const char *DoNLS (const char *);

/* encoding.c */
extern void  InitBuiltinTabs (void);
extern struct mchar *recode_mchar (struct mchar *, int, int);
extern struct mline *recode_mline (struct mline *, int, int, int);
extern int   FromUtf8 (int, int *);
extern void  AddUtf8 (int);
extern int   ToUtf8 (char *, int);
extern int   ToUtf8_comb (char *, int);
extern int   utf8_isdouble (int);
extern int   utf8_iscomb (int);
extern void  utf8_handle_comb (int, struct mchar *);
extern int   ContainsSpecialDeffont (struct mline *, int, int, int);
extern int   LoadFontTranslation (int, char *);
extern void  LoadFontTranslationsForEncoding (int);
extern void  WinSwitchEncoding (struct win *, int);
extern int   FindEncoding (char *);
extern char *EncodingName (int);
extern int   EncodingDefFont (int);
extern void  ResetEncoding (struct win *);
extern int   CanEncodeFont (int, int);
extern int   DecodeChar (int, int, int *);
extern int   RecodeBuf (unsigned char *, int, int, int, unsigned char *);
extern int   PrepareEncodedChar (int);
extern int   EncodeChar (char *, int, int, int *);

/* layout.c */
extern void  RemoveLayout (struct layout *);
extern int   LayoutDumpCanvas (struct canvas *, char *);

/*
 * GLOBAL VARIABLES
 */

extern char screenterm[];
extern char version[];
extern char DefaultShell[];
extern char HostName[];
extern char NullStr[];
extern char SockPath[];
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
extern char *zmodem_recvcmd;
extern char *zmodem_sendcmd;
extern char *ActivityString;
extern char *BellString;
extern char *BufferFile;
extern char *LoginName;
extern char *PowDetachString;
extern char *RcFileName;
extern char *ShellArgs[];
extern char *ShellProg;
extern char *SockMatch;
extern char *SockName;
extern char *VisualBellString;
extern char **environ;
extern char **NewEnv;

extern unsigned char mark_key_tab[];
extern unsigned char *blank;
extern unsigned char *null;

extern int adaptflag;
extern int attach_fd;
extern int attr2color[][4];
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
extern int eff_gid;
extern int eff_uid;
extern int focusminheight;
extern int focusminwidth;
extern int force_vt;
extern int hardcopy_append;
extern int hardstatusemu;
extern int idletimo;
extern int iflag;
extern int join_with_cr;
extern int kmap_extn;
extern int lsflag;
extern int log_flush;
extern int logtstamp_on;
extern int logtstamp_after;
extern int maxusercount;
extern int maxwin;
extern int multi_uid;
extern int multiattach;
extern int nattr2color;
extern int nversion;
extern int own_uid;
extern int pastefont;
extern int pty_preopen;
extern int search_ic;
extern int separate_sids;
extern int queryflag;
extern int quietflag;
extern int real_gid;
extern int real_uid;
extern int renditions[];
extern int rflag;
extern int tty_mode;
extern int tty_oldmode;
extern int use_altscreen;
extern int use_hardstatus;
extern int wipeflag;
extern int xflag;
extern int visual_bell;
extern int zmodem_mode;
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
extern struct display *display, *displays;
extern struct LayFuncs ListLf;
extern struct LayFuncs MarkLf;
extern struct LayFuncs WinLf;
extern struct LayFuncs BlankLf;
extern struct layout *layout_attach, *layout_last, layout_last_marker;
extern struct layout *layouts;
extern struct layout *laytab[];
extern struct NewWindow nwin_undef, nwin_default, nwin_options;
extern struct win *fore, **wtab, *console_window, *windows;
extern struct kmap_ext *kmap_exts;
extern int kmap_extn;
extern struct layer *flayer;
extern struct mline mline_blank;
extern struct mline mline_null;
extern struct mline mline_old;
extern struct mchar mchar_so;
extern struct mchar mchar_blank;
extern struct mchar mchar_null;
extern struct comm comms[];
extern struct event logflushev;
extern struct event serv_read;
extern struct mode attach_Mode;
extern struct passwd *ppp;


#ifdef NETHACK
extern int nethackflag;
#endif

#if defined(TIOCSWINSZ) || defined(TIOCGWINSZ)
extern struct winsize glwz;
#endif

#ifdef O_NOCTTY
extern int separate_sids;
#endif

# if defined(GETUTENT) && (!defined(SVR4) || defined(__hpux)) && ! defined(__CYGWIN__)
#  if defined(hpux) /* cruel hpux release 8.0 */
#   define pututline _pututline
#  endif /* hpux */
extern struct utmp *getutline(), *pututline();
#  if defined(_SEQUENT_)
extern struct utmp *ut_add_user(), *ut_delete_user();
extern char *ut_find_host();
#   ifndef UTHOST
#    define UTHOST		/* _SEQUENT_ has ut_find_host() */
#   endif
#  endif /* _SEQUENT_ */
# endif /* GETUTENT && !SVR4 */

