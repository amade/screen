#ifndef SCREEN_INPUT_H
#define SCREEN_INPUT_H

#include <stdlib.h>
#include <stdint.h>

void  inp_setprompt (uint32_t *, uint32_t *);
void  Input (uint32_t *, size_t, int, void (*)(uint32_t *, size_t, void *), char *, int);
int   InInput (void);

#endif /* SCREEN_INPUT_H */
