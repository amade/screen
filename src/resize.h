#ifndef SCREEN_RESIZE_H
#define SCREEN_RESIZE_H

int   ChangeWindowSize (Window *, int, int, int);
void  ChangeScreenSize (int, int, int);
void  CheckScreenSize (int);
void *xrealloc (void *, size_t);
void  ResizeLayersToCanvases (void);
void  ResizeLayer (Layer *, int, int, Display *);
int   MayResizeLayer (Layer *);
void  FreeAltScreen (Window *);
void  EnterAltScreen (Window *);
void  LeaveAltScreen (Window *);

/* global variables */

#if defined(TIOCSWINSZ) || defined(TIOCGWINSZ)
extern struct winsize glwz;
#endif

#endif /* SCREEN_RESIZE_H */
