/* Copyright (c) 2008 Sadrul Habib Chowdhury (sadrul@users.sf.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 ****************************************************************
 * $Id$ FAU
 */
#ifndef SCRIPT_H
#define SCRIPT_H
struct win;

/*Obsolete*/
struct ScriptFuncs
{
  int (*sf_ForeWindowChanged) __P((void));
  int (*sf_ProcessCaption) __P((const char *, struct win *, int len));
  int (*sf_CommandExecuted) __P((const char *, const char **, int));
};

/***Language binding***/
struct binding
{
  char * name;
  int inited;
  int registered;
  int (*bd_Init) __P((void));
  int (*bd_Finit) __P((void));
  int (*bd_call) __P((char *func, char **argv));
  /*Returns zero on failure, non zero on success*/
  int (*bd_Source) __P((const char *, int));
  struct binding *b_next;
  struct ScriptFuncs *fns;
};

void LoadBindings(void);
void FinializeBindings(void);
void ScriptCmd __P((int argc, const char **argv));

/***Script events***/

/* Script event listener */
struct listener 
{
  /*Binding dependent event handler data*/
  void *handler; 

  /* dispatcher provided by the binding.
   * The return value is significant: 
   * a non-zero value will stop further
   * notification to the rest of the chain.*/
  int (*dispatcher) __P((void *handler, const char *params, va_list va)); 
  
  /* smaller means higher privilege.*/
  int priv;
  struct listener *chain;
};

/* the script_event structure needs to be zeroed before using.
 * embeding this structure directly into screen objects will do the job, as
 * long as the objects are created from calloc() call.*/
struct script_event
{
  /* expected parameter description of this event. */
  char *params;
  struct listener *listeners;
};
struct script_event* object_get_event __P((char *obj, const char *name));
int trigger_sevent(struct script_event *ev, VA_DOTS);

struct gevents {
    struct script_event cmdexecuted;
    struct script_event detached;
};
extern struct gevents globalevents;
#endif
