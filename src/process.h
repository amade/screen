#ifndef SCREEN_PROCESS_H
#define SCREEN_PROCESS_H

void  InitKeytab (void);
void  ProcessInput (char *, int);
void  ProcessInput2 (char *, int);
void  DoProcess (struct win *, char **, int *, struct paster *);
void  DoAction  (struct action *, int);
int   FindCommnr (const char *);
void  DoCommand (char **, int *);
void  Activate (int);
void  KillWindow (struct win *);
void  SetForeWindow (struct win *);
int   Parse (char *, int, char **, int *);
void  SetEscape (struct acluser *, int, int);
void  DoScreen (char *, char **);
int   IsNumColon (char *, int, char *, int);
void  ShowWindows (int);
void ShowWindowsX(char *string);
char *AddWindows (char *, int, int, int);
char *AddWindowFlags (char *, int, struct win *);
char *AddOtherUsers (char *, int, struct win *);
int   WindowByNoN (char *);
struct win *FindNiceWindow (struct win *, char *);
int   CompileKeys (char *, int, unsigned char *);
void  RefreshXtermOSC (void);
int   ParseSaveStr (struct action *act, char **);
int   ParseNum (struct action *act, int *);
int   ParseSwitch (struct action *, int *);
uint64_t ParseAttrColor (char *, int);
void  ApplyAttrColor (uint64_t, struct mchar *);
void  SwitchWindow (int);
int   StuffKey (int);

#endif /* SCREEN_PROCESS_H */
