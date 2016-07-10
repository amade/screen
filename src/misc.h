#ifndef SCREEN_MISC_H
#define SCREEN_MISC_H

#include "config.h"

#include <sys/types.h>

#include "image.h"

uint32_t *SaveStr (const uint32_t *);
uint32_t *SaveStrn (const uint32_t *, size_t);
#ifndef HAVE_STRERROR
char *strerror (int);
#endif
void  centerline (char *, int);
void  leftline (char *, int, struct mchar *);
char *Filename (char *);
char *stripdev (char *);
void  closeallfiles (int);
int   UserContext (void);
void  UserReturn (int);
int   UserStatus (void);
void (*xsignal (int, void (*)(int))) (int);
#if defined(HAVE_SETEUID) || defined(HAVE_SETREUID)
void  xseteuid  (int);
void  xsetegid  (int);
#endif
int   AddXChar (char *, int);
int   AddXChars (char *, int, char *);
void  sleep1000 (int);

#endif /* SCREEN_MISC_H */
