#ifndef SCREEN_TELNET_H
#define SCREEN_TELNET_H

#include "config.h"

#include "window.h"

#ifdef ENABLE_TELNET
int TelOpenAndConnect(Window *);
int TelIsline(Window *);
void TelProcessLine(char **, size_t *);
int DoTelnet(char *, int *, int);
int TelIn(Window *, char *, int, int);
void TelBreak(Window *);
void TelWindowSize(Window *);
void TelStatus(Window *, char *, size_t);
#endif

#endif /* SCREEN_TELNET_H */
