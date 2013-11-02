#ifndef SCREEN_INPUT_H
#define SCREEN_INPUT_H

void  inp_setprompt (char *, char *);
void  Input (char *, int, int, void (*)(char *, int, void *), char *, int);
int   InInput (void);

#endif /* SCREEN_INPUT_H */
