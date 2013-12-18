#ifndef SCREEN_ALIAS_H
#define SCREEN_ALIAS_H

typedef struct Alias Alias;
struct Alias {
	int nr;
	char *name;   /* Name of the alias */
	int cmdnr;    /* Number of the command this is alias for */
	char **args;  /* The argument list for the command */
	int *argl;
	Alias *next;
};

/* global list of aliases */
extern Alias *g_aliases_list;

void AddAlias(const char *name, const char *value, char **args, int *argl, int count);
void DelAlias(const char *name);

Alias *FindAlias(const char *name);
Alias *FindAliasnr(int nr);

int DoAlias(const char *name, char **args, int *argl);

#endif /* SCREEN_ALIAS_H */
