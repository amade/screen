#ifndef SCREEN_MISC_H
#define SCREEN_MISC_H

char *SaveStr (const char *);
char *SaveStrn (const char *, int);
char *InStr (char *, const char *);
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
int   AddXChar (char *, int);
int   AddXChars (char *, int, char *);
void  sleep1000 (int);

#endif /* SCREEN_MISC_H */
