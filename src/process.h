#ifndef SCREEN_PROCESS_H
#define SCREEN_PROCESS_H

#include "winmsgbuf.h"

void  InitKeytab (void);
void  ProcessInput (uint32_t *, size_t);
void  ProcessInput2 (uint32_t *, size_t);
void  DoProcess (Window *, uint32_t **, size_t *, struct paster *);
void  DoAction  (struct action *, int);
int   FindCommnr (const uint32_t *);
void  DoCommand (uint32_t **, int *);
void  Activate (int);
void  KillWindow (Window *);
void  SetForeWindow (Window *);
int   Parse (uint32_t *, int, uint32_t **, int *);
void  SetEscape (struct acluser *, int, int);
void  DoScreen (char *, char **);
int   IsNumColon (char *, char *, int);
void  ShowWindows (int);
char *AddWindows (WinMsgBufContext *, int, int, int);
char *AddWindowFlags (uint32_t *, int, Window *);
char *AddOtherUsers (char *, int, Window *);
int   WindowByNoN (uint32_t *);
Window *FindNiceWindow (Window *, char *);
int   CompileKeys (char *, int, unsigned char *);
void  RefreshXtermOSC (void);
uint64_t ParseAttrColor (uint32_t *, int);
void  ApplyAttrColor (uint64_t, struct mchar *);
void  SwitchWindow (int);
int   StuffKey (int);

/* global variables */

extern bool hardcopy_append;

extern uint32_t *noargs[];
extern char NullStr[];
extern char *zmodem_recvcmd;
extern char *zmodem_sendcmd;

extern int idletimo;
extern int kmap_extn;
extern int zmodem_mode;
extern int TtyMode;

extern struct action idleaction;
extern struct action dmtab[];
extern struct action ktab[];
extern struct action mmtab[];
extern struct action umtab[];

extern struct kmap_ext *kmap_exts;

#endif /* SCREEN_PROCESS_H */
