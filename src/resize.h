#ifndef SCREEN_RESIZE_H
#define SCREEN_RESIZE_H

int   ChangeWindowSize (struct win *, int, int, int);
void  ChangeScreenSize (int, int, int);
void  CheckScreenSize (int);
void *xrealloc (void *, size_t);
void  ResizeLayersToCanvases (void);
void  ResizeLayer (struct layer *, int, int, struct display *);
int   MayResizeLayer (struct layer *);
void  FreeAltScreen (struct win *);
void  EnterAltScreen (struct win *);
void  LeaveAltScreen (struct win *);

#endif /* SCREEN_RESIZE_H */
