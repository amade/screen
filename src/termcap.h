#ifndef SCREEN_TERMCAP_H
#define SCREEN_TERMCAP_H

int   InitTermcap (int, int);
char *MakeTermcap (int);
char *gettermcapstring (char *);
int   remap (int, int);
void  CheckEscape (void);
int   CreateTransTable (char *);
void  FreeTransTable (void);

#endif /* SCREEN_TERMCAP_H */
