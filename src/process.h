#ifndef SCREEN_PROCESS_H
#define SCREEN_PROCESS_H

#include "winmsgbuf.h"

void  InitKeytab (void);
void  ProcessInput (char *, int);
void  ProcessInput2 (char *, int);
void  DoProcess (Window *, char **, int *, struct paster *);
void  DoAction  (struct action *, int);
int   FindCommnr (const char *);
void  DoCommand (char **, int *);
void  Activate (int);
void  KillWindow (Window *);
void  SetForeWindow (Window *);
int   Parse (char *, int, char **, int *);
void  SetEscape (int, int);
void  DoScreen (char *, char **);
int   IsNumColon (char *, int, char *, int);
void  ShowWindows (int);
void ShowWindowsX(char *string);
char *AddWindows (WinMsgBufContext *, int, int, int);
char *AddWindowFlags (char *, int, Window *);
int   WindowByNoN (char *);
Window *FindNiceWindow (Window *, char *);
int   CompileKeys (char *, int, unsigned char *);
void  RefreshXtermOSC (void);
int   ParseSaveStr (struct action *act, char **);
int   ParseNum (struct action *act, int *);
int   ParseSwitch (struct action *, int *);
uint64_t ParseAttrColor (char *, int);
void  ApplyAttrColor (uint64_t, struct mchar *);
void  SwitchWindow (int);
int   StuffKey (int);

/* global variables */

extern char *noargs[];

extern int hardcopy_append;
extern int idletimo;
extern int kmap_extn;
extern int TtyMode;

extern struct action idleaction;
extern struct action dmtab[];
extern struct action ktab[];
extern struct action mmtab[];
extern struct action umtab[];

extern struct kmap_ext *kmap_exts;

#endif /* SCREEN_PROCESS_H */
