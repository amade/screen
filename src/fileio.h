#ifndef SCREEN_FILEIO_H
#define SCREEN_FILEIO_H

int   StartRc (char *, int);
void  FinishRc (char *);
void  RcLine (char *, int);
void  WriteFile (char *, int);
char *ReadFile (char *, int *);
void  KillBuffers (void);
int   printpipe (Window *, char *);
int   readpipe (char **);
void  RunBlanker (char **);
void  do_source (char *);

/* global variables */

extern char *rc_name;

#endif
