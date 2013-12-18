#include <stddef.h>
#include "alias.h"
#include "comm.h"
#include "fileio.h"
#include "misc.h"
#include "process.h"

Alias *g_aliases_list = NULL;

/**
 * Add an alias
 */
void AddAlias(const char *name, const char *value, char **args, int *argl, int count)
{
	Alias *nalias = NULL;
	static int next_command = RC_LAST;
	int nr;

	/* Make sure we don't already have this alias name defined. */
	if (FindAlias(name) != NULL) {
		Msg(0, "alias already defined: %s", name);
		return;
	}

	/* Make sure the alias maps to something */
	if ((nr = FindCommnr((char *)value)) == RC_ILLEGAL) {
		Alias *alias = FindAlias(value);
		if (!alias) {
			Msg(0, "%s: could not find command or alias '%s'", rc_name, value);
			return;
		}
		nr = alias->nr;
	}

	nalias = (Alias *)calloc(1, sizeof(Alias));

	/* store it */
	nalias->next = NULL;
	nalias->name = SaveStr(name);
	nalias->cmdnr = nr;
	if (count > 0) {
		nalias->args = SaveArgs(args);
		nalias->argl = calloc(count + 1, sizeof(int));
		while (count--)
			nalias->argl[count] = argl[count];
	}
	nalias->nr = ++next_command;

	/* Add to head */
	nalias->next = g_aliases_list;
	g_aliases_list = nalias;
}


/**
 * Find an alias by name.
 */
Alias *FindAlias(const char *name)
{
	Alias *t = g_aliases_list;

	while(t != NULL) {
		if ((t->name != NULL) && (strcmp(t->name, name) == 0))
			return t;

		t = t->next;
	}

	return NULL;
}

/**
 * Find an alias by number.
 */
Alias *FindAliasnr(int nr)
{
	Alias *t;
	for (t = g_aliases_list; t; t = t->next) {
		if (t->nr == nr)
			return t;
	}

	return NULL;
}

/**
 * Delete an alias
 */
void DelAlias(const char *name)
{
	/* Find the previous alias */
	Alias *cur  = g_aliases_list;
	Alias **pcur = &g_aliases_list;

	while (cur != NULL) {
		if ((cur->name != NULL) && (strcmp(cur->name, name) == 0)) {
			Alias *found = cur;
			int c;

			/* remove this one from the chain. */
			*pcur = found->next;

			free(found->name);
			if (found->args) {
				for (c = 0; found->args[c]; c++)
					free(found->args[c]);
				free(found->args);
				free(found->argl);
			}
			free(found);

			Msg(0, "alias %s removed", name);
			return;
		}
		pcur = &cur->next;
		cur = cur->next;
	}

	Msg(0, "alias %s not found", name);
}

int DoAlias(const char *name, char **args, int *argl)
{
	char **mergeds;
	int *mergedl;
	int count, i;
	Alias *alias = FindAlias(name);

	if (alias == NULL)
		return 0;

	count = 0;
	for (; args && args[count]; count++)
	;
	for (i = 0; alias->args && alias->args[i]; i++, count++)
	;
	++count;

	if ((mergeds = malloc(count * sizeof(char *))) == 0)
		return 0;
	if ((mergedl = malloc(count * sizeof(int))) == 0) {
		free(mergeds);
		return 0;
	}
	for (count = 0; alias->args && alias->args[count]; count++) {
		mergeds[count] = alias->args[count];
		mergedl[count] = alias->argl[count];
	}
	for (i = 0; args && args[i]; i++, count++) {
		mergeds[count] = args[i];
		mergedl[count] = argl[i];
	}
	mergeds[count] = 0;
	mergedl[count] = 0 ;

	DoCommand(mergeds, mergedl);

	free(mergeds);
	free(mergedl);
	return 1;
}
