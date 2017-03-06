#ifndef SCREEN_FILEIO_H
#define SCREEN_FILEIO_H

#include "window.h"

int   StartRc (char *, int);
void  FinishRc (char *);
void  RcLine (uint32_t *, int);
FILE *secfopen (char *, char *);
int   secopen (char *, int, int);
void  WriteFile (struct acluser *, char *, int);
char *ReadFile (char *, int *);
void  KillBuffers (void);
int   printpipe (Window *, char *);
int   readpipe (char **);
void  RunBlanker (char **);
void  do_source (char *);

/* global variables */

extern char *rc_name;

#endif
