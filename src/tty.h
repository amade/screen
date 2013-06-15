#ifndef SCREEN_TTY_H
#define SCREEN_TTY_H

int   OpenTTY (char *, char *);
void  InitTTY (struct mode *, int);
void  GetTTY (int, struct mode *);
void  SetTTY (int, struct mode *);
void  SetMode (struct mode *, struct mode *, int, int);
void  SetFlow (int);
void  SendBreak (struct win *, int, int);
int   TtyGrabConsole (int, int, char *);
char *TtyGetModemStatus (int, char *);
int   fgtty (int);
void  brktty (int);
struct baud_values *lookup_baud (int bps);
int   SetBaud (struct mode *, int, int);
int   SttyMode (struct mode *, char *);
int   CheckTtyname (char *);

#endif /* SCREEN_TTY_H */
